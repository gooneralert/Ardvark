#pragma once

#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/classes/Classes.h"
#include "app/Settings.h"
#include "../protocol/Json.h"

#include <cctype>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace Cheat {
namespace Features {
namespace McpBridge {
namespace detail {

inline Instance DataModelRoot()
{
	if (!g_Memory.IsAttached())
		return {};

	const auto& dm = Globals::InstanceDataModel;
	if (!g_Memory.IsValid(dm.address))
		return {};

	return Instance(dm.address);
}

inline Instance ResolvePath(const Instance& root, const std::string& path)
{
	if (!root.address)
		return {};

	if (path.empty() || path == "/" || path == ".")
		return root;

	Instance cur = root;
	std::size_t i = 0;
	if (!path.empty() && (path[0] == '/' || path[0] == '\\'))
		i = 1;

	while (i < path.size())
	{
		std::size_t j = path.find_first_of("/\\", i);
		if (j == std::string::npos)
			j = path.size();

		std::string part = path.substr(i, j - i);
		i = j + 1;

		if (part.empty() || part == ".")
			continue;

		if (part == "game" || part == "Game" || part == "DataModel")
			continue;

		auto child = cur.FindFirstChild(part);
		if (!child || !g_Memory.IsValid(child->address))
			return {};

		cur = *child;
	}

	return cur;
}

inline Instance ResolveTarget(const std::string& query)
{
	Instance root = DataModelRoot();
	if (!root.address)
		return {};

	std::string addr_s = QueryParam(query, "addr");
	if (!addr_s.empty())
	{
		std::uint64_t a = ParseAddr(addr_s);
		if (g_Memory.IsValid(a))
			return Instance(a);

		return {};
	}

	std::string path = QueryParam(query, "path");
	if (!path.empty())
		return ResolvePath(root, path);

	// по дефолту workspace, иначе dm
	if (Globals::Workspace && g_Memory.IsValid(Globals::Workspace->address))
		return Instance(Globals::Workspace->address);

	return root;
}

inline std::string HandleHealth()
{
	bool attached = g_Memory.IsAttached();
	const auto& dm = Globals::InstanceDataModel;

	std::uint64_t place = 0;
	std::uint64_t game = 0;
	if (attached && g_Memory.IsValid(dm.address))
	{
		place = dm.GetPlaceId();
		game = dm.GetGameId();
	}

	const char* att = "false";
	if (attached)
		att = "true";

	std::ostringstream o;
	o << "{\"ok\":true,\"attached\":" << att
	  << ",\"placeId\":" << place
	  << ",\"gameId\":" << game << "}";
	return o.str();
}

inline std::string HandleInfo()
{
	if (!g_Memory.IsAttached())
		return "{\"error\":\"not_attached\"}";

	const auto& dm = Globals::InstanceDataModel;
	if (!g_Memory.IsValid(dm.address))
		return "{\"error\":\"no_datamodel\"}";

	const char* loaded = "false";
	if (dm.IsGameLoaded())
		loaded = "true";

	std::ostringstream o;
	o << "{\"placeId\":" << dm.GetPlaceId()
	  << ",\"gameId\":" << dm.GetGameId()
	  << ",\"placeVersion\":" << dm.GetPlaceVersion()
	  << ",\"creatorId\":" << dm.GetCreatorId()
	  << ",\"jobId\":\"" << JsonEscape(dm.GetJobId()) << "\""
	  << ",\"gameLoaded\":" << loaded
	  << ",\"dataModel\":\"0x" << std::hex
	  << (unsigned long long)dm.address << std::dec << "\"";

	if (Globals::Workspace && g_Memory.IsValid(Globals::Workspace->address))
	{
		o << ",\"workspace\":\"0x" << std::hex
		  << (unsigned long long)Globals::Workspace->address
		  << std::dec << "\"";
	}

	o << "}";
	return o.str();
}

inline std::string HandleChildren(const std::string& query)
{
	Instance target = ResolveTarget(query);
	if (!g_Memory.IsValid(target.address))
		return "{\"error\":\"not_found\"}";

	auto kids = target.GetChildren();
	int n = (int)kids.size();
	if (n > 256)
		n = 256;

	std::ostringstream o;
	o << "{\"parent\":" << InstanceJson(target) << ",\"count\":";
	o << kids.size() << ",\"children\":[";
	for (int i = 0; i < n; ++i)
	{
		if (i)
			o << ',';
		o << InstanceJson(kids[(std::size_t)i]);
	}

	if ((int)kids.size() > n)
		o << "],\"truncated\":true}";

	else
		o << "],\"truncated\":false}";

	return o.str();
}

inline void SearchRecurse(const Instance& node, const std::string& needle_lower,
                   std::vector<Instance>& out, int depth)
{
	// depth 8 / 80 hits дальше нет смысла
	if (depth > 8 || (int)out.size() >= 80)
		return;

	if (!g_Memory.IsValid(node.address))
		return;

	auto kids = node.GetChildren();
	for (const auto& c : kids)
	{
		if ((int)out.size() >= 80)
			return;

		if (!g_Memory.IsValid(c.address))
			continue;

		std::string name = c.GetName();
		std::string lower = name;
		for (char& ch : lower)
			ch = (char)std::tolower((unsigned char)ch);

		if (lower.find(needle_lower) != std::string::npos)
			out.push_back(c);

		SearchRecurse(c, needle_lower, out, depth + 1);
	}
}

inline std::string HandleSearch(const std::string& query)
{
	std::string name = QueryParam(query, "name");
	if (name.empty())
		return "{\"error\":\"missing_name\"}";

	int limit = 80;
	std::string lim = QueryParam(query, "limit");
	if (!lim.empty())
	{
		try
		{
			limit = std::stoi(lim);
		}
		catch (...) {}

		if (limit < 1) limit = 1;
		if (limit > 80) limit = 80;
	}

	Instance root = DataModelRoot();
	std::string path = QueryParam(query, "path");
	if (!path.empty())
	{
		root = ResolvePath(root, path);
	}

	else if (Globals::Workspace && g_Memory.IsValid(Globals::Workspace->address))
	{
		root = Instance(Globals::Workspace->address);
	}

	if (!g_Memory.IsValid(root.address))
		return "{\"error\":\"no_root\"}";

	std::string needle = name;
	for (char& ch : needle)
		ch = (char)std::tolower((unsigned char)ch);

	std::vector<Instance> hits;
	hits.reserve((std::size_t)limit);
	SearchRecurse(root, needle, hits, 0);

	std::ostringstream o;
	o << "{\"query\":\"" << JsonEscape(name) << "\",\"count\":" << hits.size() << ",\"results\":[";

	int n = (int)hits.size();
	if (n > limit)
		n = limit;

	for (int i = 0; i < n; ++i)
	{
		if (i)
			o << ',';
		o << InstanceJson(hits[(std::size_t)i]);
	}

	o << "]}";
	return o.str();
}

inline std::string HandlePath(const std::string& query)
{
	Instance target = ResolveTarget(query);
	if (!g_Memory.IsValid(target.address))
		return "{\"error\":\"not_found\"}";

	// вверх по parent до dm
	std::vector<std::string> parts;
	Instance cur = target;
	for (int guard = 0; guard < 64 && g_Memory.IsValid(cur.address); ++guard)
	{
		parts.push_back(cur.GetName());
		auto parent = cur.GetParent();
		if (!parent || !g_Memory.IsValid(parent->address))
			break;

		if (parent->address == Globals::InstanceDataModel.address)
		{
			parts.push_back("DataModel");
			break;
		}

		cur = *parent;
	}

	std::string path;
	for (auto it = parts.rbegin(); it != parts.rend(); ++it)
	{
		if (!path.empty())
			path += '/';
		path += *it;
	}

	std::ostringstream o;
	o << "{\"instance\":" << InstanceJson(target)
	  << ",\"path\":\"" << JsonEscape(path) << "\"}";
	return o.str();
}

inline std::string Dispatch(const std::string& path, const std::string& query)
{
	if (!Cheat::g_Settings.misc.mcp)
		return "{\"error\":\"mcp_disabled\"}";

	if (path == "/health")
		return HandleHealth();

	if (path == "/info")
		return HandleInfo();

	if (path == "/children")
		return HandleChildren(query);

	if (path == "/search")
		return HandleSearch(query);

	if (path == "/path")
		return HandlePath(query);

	return "{\"error\":\"unknown_endpoint\"}";
}

}
}
}
}
