// Fission in-process — без Server.exe. C++latest, без pch.
#include "FissionEmbed.h"

#include "Decompiler.hpp"

#include "Luau/Common.h"
#include "libassert/assert.hpp"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Cheat {
namespace Features {
namespace FissionEmbed {
namespace {

constexpr size_t k_max_beautify = 2u << 20;
// std::regex у MSVC рекурсивен по числу повторений — длинную строку не отдаём движку
constexpr size_t k_max_line = 1024;

void EnableLuauFFlags()
{
	static bool done = false;
	if (done)
		return;
	for (Luau::FValue<bool>* flag = Luau::FValue<bool>::list; flag; flag = flag->next)
	{
		if (flag->name && std::strncmp(flag->name, "Luau", 4) == 0)
			flag->value = true;
	}
	// assert → throw, не abort весь процесс
	libassert::set_failure_handler([](const libassert::assertion_info& info) -> void {
		throw std::runtime_error(info.to_string(0, libassert::color_scheme::blank));
	});
	done = true;
}

// fission срёт в cout "generated source code" — глушим
struct CoutMute
{
	std::ostringstream sink;
	std::streambuf* prev;
	CoutMute() : prev(std::cout.rdbuf(sink.rdbuf())) {}
	~CoutMute() { std::cout.rdbuf(prev); }
};

bool TryReadDecByte(const std::string& src, size_t at, int& val, size_t& end)
{
	if (at >= src.size() || src[at] < '0' || src[at] > '9')
		return false;
	val = 0;
	size_t j = at;
	int digits = 0;
	while (j < src.size() && digits < 3 && src[j] >= '0' && src[j] <= '9')
	{
		val = val * 10 + (src[j] - '0');
		++j;
		++digits;
	}
	if (digits == 0 || val > 255)
		return false;
	end = j;
	return true;
}

bool IsIdentChar(char c)
{
	return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

template <class F>
void EachLine(const char* b, const char* e, F fn)
{
	while (b < e)
	{
		const char* nl = static_cast<const char*>(std::memchr(b, '\n', static_cast<size_t>(e - b)));
		const char* le = nl ? nl + 1 : e;
		fn(b, le);
		b = le;
	}
}

// весь пост-процесс идёт построчно: regex по мегабайтному буферу — и квадрат, и переполнение стека
template <class F>
void ScanLines(const std::string& view, const std::regex& re, std::string_view needle, F fn)
{
	EachLine(view.data(), view.data() + view.size(), [&](const char* b, const char* e) {
		const size_t len = static_cast<size_t>(e - b);
		if (len > k_max_line)
			return;
		if (!needle.empty() && std::string_view(b, len).find(needle) == std::string_view::npos)
			return;
		for (std::cregex_iterator it(b, e, re), end; it != end; ++it)
			fn(*it);
	});
}

void ReplaceAll(std::string& s, std::string_view from, std::string_view to)
{
	size_t pos = s.find(from);
	if (from.empty() || pos == std::string::npos)
		return;
	std::string out;
	out.reserve(s.size());
	size_t prev = 0;
	while (pos != std::string::npos)
	{
		out.append(s, prev, pos - prev);
		out.append(to);
		prev = pos + from.size();
		pos = s.find(from, prev);
	}
	out.append(s, prev, std::string::npos);
	s.swap(out);
}

// один проход лексера вместо find() по всему тексту на каждое имя
struct NameSet
{
	std::unordered_set<std::string_view> names;

	explicit NameSet(const std::string& src)
	{
		const size_t n = src.size();
		names.reserve(n / 48 + 16);
		for (size_t i = 0; i < n;)
		{
			if (!IsIdentChar(src[i]))
			{
				++i;
				continue;
			}
			const size_t s = i;
			while (i < n && IsIdentChar(src[i]))
				++i;
			const char prev = s ? src[s - 1] : '\0';
			const char next = i < n ? src[i] : '\0';
			if (prev == '.' || prev == ':' || prev == '"' || prev == '\'' || next == '"' || next == '\'')
				continue;
			names.emplace(src.data() + s, i - s);
		}
	}

	bool Has(const std::string& n) const { return names.count(std::string_view(n)) != 0; }
};

void StripTypeAnnotations(std::string& src)
{
	static const std::regex res[] = {
		std::regex(R"((local\s+[A-Za-z_][\w]*)\s*:\s*[A-Za-z_][\w|]*)"),
		std::regex(R"((,\s*[A-Za-z_][\w]*)\s*:\s*[A-Za-z_][\w|]*)"),
		std::regex(R"((\(\s*[A-Za-z_][\w]*)\s*:\s*[A-Za-z_][\w|]*)"),
		std::regex(R"(\)\s*:\s*[A-Za-z_][\w|]*(?=\s*(?:end|;|\r|\n|$)))"),
	};
	static const char* const reps[] = { "$1", "$1", "$1", ")" };

	std::string out, line, tmp;
	out.reserve(src.size());
	EachLine(src.data(), src.data() + src.size(), [&](const char* b, const char* e) {
		const size_t len = static_cast<size_t>(e - b);
		if (len > k_max_line || !std::memchr(b, ':', len))
		{
			out.append(b, len);
			return;
		}
		line.assign(b, len);
		for (size_t i = 0; i < std::size(res); ++i)
		{
			tmp.clear();
			std::regex_replace(std::back_inserter(tmp), line.cbegin(), line.cend(), res[i], reps[i]);
			line.swap(tmp);
		}
		out += line;
	});
	src.swap(out);
}

void FilterLines(std::string& src, const std::regex& drop)
{
	std::string out;
	out.reserve(src.size());
	EachLine(src.data(), src.data() + src.size(), [&](const char* b, const char* e) {
		const char* t = e;
		while (t > b && (t[-1] == '\n' || t[-1] == '\r'))
			--t;
		if (static_cast<size_t>(t - b) <= k_max_line && std::regex_match(b, t, drop))
			return;
		out.append(b, static_cast<size_t>(e - b));
	});
	src.swap(out);
}

void RemoveDeadNilLocals(std::string& src)
{
	static const std::regex re(R"([ \t]*local\s+v\d+\s*=\s*nil\s*)");
	static const std::regex kill(R"([ \t]*v\d+\s*=\s*nil\s*)");
	FilterLines(src, re);
	FilterLines(src, kill);
}

void RemovePcallKills(std::string& src)
{
	static const std::regex re(
		R"([ \t]*(ok(?:_\d+)?|result(?:_\d+)?)\s*=\s*(?:""|''|nil|true|false|\d+)\s*)");
	FilterLines(src, re);
}

std::string LastIdent(const std::string& path)
{
	size_t dot = path.find_last_of('.');
	std::string leaf = (dot == std::string::npos) ? path : path.substr(dot + 1);
	size_t slash = leaf.find_last_of("/\\");
	if (slash != std::string::npos)
		leaf = leaf.substr(slash + 1);
	if (leaf.size() > 4 && leaf.substr(leaf.size() - 4) == ".lua")
		leaf = leaf.substr(0, leaf.size() - 4);
	for (char& c : leaf)
	{
		if (!IsIdentChar(c))
			c = '_';
	}
	if (leaf.empty() || std::isdigit(static_cast<unsigned char>(leaf[0])))
		leaf = "_" + leaf;
	return leaf;
}

void ApplyRenames(std::string& src, const std::vector<std::pair<std::string, std::string>>& renames)
{
	std::unordered_map<std::string_view, const std::string*> map;
	map.reserve(renames.size());
	for (const auto& r : renames)
	{
		if (!r.first.empty() && r.first != r.second)
			map.emplace(std::string_view(r.first), &r.second);
	}
	if (map.empty())
		return;

	std::string out;
	out.reserve(src.size());
	const size_t n = src.size();
	for (size_t i = 0; i < n;)
	{
		if (!IsIdentChar(src[i]))
		{
			out.push_back(src[i]);
			++i;
			continue;
		}
		const size_t s = i;
		while (i < n && IsIdentChar(src[i]))
			++i;
		const auto f = map.find(std::string_view(src.data() + s, i - s));
		if (f != map.end())
			out += *f->second;
		else
			out.append(src, s, i - s);
	}
	src.swap(out);
}

std::string TakeName(const NameSet& taken, std::unordered_set<std::string>& claimed, const std::string& base)
{
	std::string n = base;
	int i = 2;
	while (taken.Has(n) || claimed.count(n))
	{
		n = base + "_" + std::to_string(i);
		++i;
	}
	claimed.insert(n);
	return n;
}

void BeautifyPcallsSimple(const std::string& view, std::string& target)
{
	static const std::regex re(
		R"(local\s+(v\d+(?:_\d+)?)\s*,\s*(v\d+(?:_\d+)?)\s*=\s*(?:x?pcall|coroutine\.resume)\s*\()");
	std::vector<std::pair<std::string, std::string>> renames;
	std::unordered_set<std::string> claimed;
	std::unordered_set<std::string> seen;
	{
		const NameSet taken(target);
		ScanLines(view, re, "local", [&](const std::cmatch& m) {
			std::string a = m[1].str();
			std::string b = m[2].str();
			if (seen.count(a) || seen.count(b))
				return;
			seen.insert(a);
			seen.insert(b);
			renames.emplace_back(std::move(a), TakeName(taken, claimed, "ok"));
			renames.emplace_back(std::move(b), TakeName(taken, claimed, "result"));
		});
	}
	std::string scan = view;
	ApplyRenames(target, renames);
	ApplyRenames(scan, renames);

	static const std::regex and_re(
		R"(local\s+(v\d+(?:_\d+)?)\s*=\s*ok(?:_\d+)?\s+and\s+result(?:_\d+)?)");
	std::vector<std::pair<std::string, std::string>> feats;
	const NameSet taken(target);
	ScanLines(scan, and_re, "local", [&](const std::cmatch& m) {
		std::string old = m[1].str();
		if (!seen.insert(old).second)
			return;
		feats.emplace_back(std::move(old), TakeName(taken, claimed, "feature"));
	});
	ApplyRenames(target, feats);
}

void RenameRequires(const std::string& view, std::string& target)
{
	static const std::regex re_path(R"(local\s+(v\d+(?:_\d+)?)\s*=\s*require\s*\(\s*([\w.]+)\s*\))");
	static const std::regex re_str(R"(local\s+(v\d+(?:_\d+)?)\s*=\s*require\s*\(\s*[\"']([^\"']+)[\"']\s*\))");
	std::vector<std::pair<std::string, std::string>> renames;
	std::unordered_set<std::string> claimed;
	const NameSet taken(target);

	auto grab = [&](const std::cmatch& m) {
		renames.emplace_back(m[1].str(), TakeName(taken, claimed, LastIdent(m[2].str())));
	};
	ScanLines(view, re_path, "require", grab);
	ScanLines(view, re_str, "require", grab);

	ApplyRenames(target, renames);
}

void RenameMemberLocals(const std::string& view, std::string& target)
{
	static const std::regex re(R"(local\s+(v\d+(?:_\d+)?)\s*=\s*([\w.]+)\.([A-Za-z_][\w]*)\s*(?:\r?\n|;))");
	std::vector<std::pair<std::string, std::string>> renames;
	std::unordered_set<std::string> claimed;
	std::unordered_set<std::string> seen;
	const NameSet taken(target);
	ScanLines(view, re, "local", [&](const std::cmatch& m) {
		std::string old = m[1].str();
		const std::string leaf = m[3].str();
		if (leaf.size() < 2 || leaf == "new" || leaf == "connect" || leaf == "Connect")
			return;
		if (!seen.insert(old).second)
			return;
		renames.emplace_back(std::move(old), TakeName(taken, claimed, leaf));
	});
	ApplyRenames(target, renames);
}

void RenameArgs(const std::string& view, std::string& target)
{
	struct Hint { const char* field; const char* name; };
	static const Hint hints[] = {
		{ "loading", "query" },
		{ "data", "query" },
		{ "error", "query" },
		{ "fetching", "query" },
		{ "Document", "props" },
		{ "navigation", "props" },
		{ "Character", "player" },
		{ "UserId", "player" },
		{ "AbsoluteSize", "frame" },
		{ "Parent", "instance" },
	};

	std::unordered_map<std::string, std::unordered_map<std::string, int>> votes;
	static const std::regex use_re(R"(\b(arg\d+)\.([A-Za-z_][\w]*))");
	ScanLines(view, use_re, "arg", [&](const std::cmatch& m) {
		const std::string arg = m[1].str();
		const std::string field = m[2].str();
		bool hit = false;
		for (const auto& h : hints)
		{
			if (field == h.field)
			{
				votes[arg][h.name]++;
				hit = true;
			}
		}
		if (!hit)
			votes[arg]["value"]++;
	});

	std::vector<std::pair<std::string, std::string>> renames;
	std::unordered_set<std::string> claimed;
	std::unordered_set<std::string> have;
	const NameSet taken(target);
	for (const auto& kv : votes)
	{
		std::string best = "value";
		int best_n = 0;
		for (const auto& v : kv.second)
		{
			if (v.second > best_n)
			{
				best_n = v.second;
				best = v.first;
			}
		}
		have.insert(kv.first);
		renames.emplace_back(kv.first, TakeName(taken, claimed, best));
	}

	static const std::regex any_arg(R"(\barg(\d+)\b)");
	ScanLines(view, any_arg, "arg", [&](const std::cmatch& m) {
		std::string arg = m[0].str();
		if (!have.insert(arg).second)
			return;
		static const char letters[] = "abcdefghij";
		// stoi бросает на arg99999999999 и валит весь декомпил
		const unsigned long idx = std::strtoul(m[1].first, nullptr, 10);
		renames.emplace_back(std::move(arg), TakeName(taken, claimed, std::string(1, letters[idx % 10])));
	});

	ApplyRenames(target, renames);
}

void RenameFromAssignments(const std::string& view, std::string& target)
{
	struct Rule { const std::regex re; const char* name; };
	static const Rule rules[] = {
		{ std::regex(R"(local\s+(v\d+(?:_\d+)?)\s*=\s*script\.Parent\b)"), "Character" },
		{ std::regex(R"(local\s+(v\d+(?:_\d+)?)\s*=\s*["']Standing["'])"), "currentPose" },
	};
	{
		std::vector<std::pair<std::string, std::string>> renames;
		std::unordered_set<std::string> claimed;
		const NameSet taken(target);
		for (const auto& rule : rules)
		{
			std::string old;
			ScanLines(view, rule.re, "local", [&](const std::cmatch& m) {
				if (old.empty())
					old = m[1].str();
			});
			if (!old.empty())
				renames.emplace_back(std::move(old), TakeName(taken, claimed, rule.name));
		}
		ApplyRenames(target, renames);
	}

	RenameRequires(view, target);
	RenameMemberLocals(view, target);
	RenameArgs(view, target);
}

void RenameAnimTables(const std::string& view, std::string& target)
{
	static const std::regex field_re(R"(\b(v\d+(?:_\d+)?)\.(idle|walk|run|swim|jump|fall|climb|sit|wave|dance)\b)");
	std::unordered_map<std::string, int> hits;
	ScanLines(view, field_re, {}, [&](const std::cmatch& m) { hits[m[1].str()]++; });

	std::string best;
	int best_n = 0;
	for (const auto& kv : hits)
	{
		if (kv.second > best_n)
		{
			best_n = kv.second;
			best = kv.first;
		}
	}

	std::vector<std::pair<std::string, std::string>> renames;
	std::unordered_set<std::string> claimed;
	const NameSet taken(target);
	if (best_n >= 3)
		renames.emplace_back(std::move(best), TakeName(taken, claimed, "Animations"));

	static const std::regex emote_re(
		R"(local\s+(v\d+(?:_\d+)?)\s*=\s*\{\s*wave\s*=\s*(?:true|false))");
	std::string emote;
	ScanLines(view, emote_re, "local", [&](const std::cmatch& m) {
		if (emote.empty())
			emote = m[1].str();
	});
	if (!emote.empty())
		renames.emplace_back(std::move(emote), TakeName(taken, claimed, "emoteNames"));

	ApplyRenames(target, renames);
}

std::unordered_set<std::string_view> DeclaredNames(const std::string& view)
{
	std::unordered_set<std::string_view> names;
	struct Decl { const std::regex re; const char* needle; };
	static const Decl decls[] = {
		{ std::regex(R"(\blocal\s+function\s+([A-Za-z_]\w*))"), "local" },
		{ std::regex(R"(\blocal\s+([A-Za-z_]\w*(?:\s*,\s*[A-Za-z_]\w*)*))"), "local" },
		{ std::regex(R"(\bfor\s+([A-Za-z_]\w*(?:\s*,\s*[A-Za-z_]\w*)*))"), "for" },
		{ std::regex(R"(\bfunction\s*[\w.:]*\s*\(([^)\r\n]*)\))"), "function" },
	};
	static const std::regex ident(R"([A-Za-z_]\w*)");
	for (const auto& d : decls)
	{
		ScanLines(view, d.re, d.needle, [&](const std::cmatch& m) {
			for (std::cregex_iterator jt(m[1].first, m[1].second, ident), jend; jt != jend; ++jt)
				names.emplace((*jt)[0].first, static_cast<size_t>((*jt)[0].length()));
		});
	}
	return names;
}

void StripFissionSuffixes(const std::string& view, std::string& target)
{
	static const std::regex re(R"(\b([A-Za-z]+\d*)(?:_\d+)+\b)");
	const auto declared = DeclaredNames(view);
	const NameSet taken(target);
	std::vector<std::pair<std::string, std::string>> renames;
	std::unordered_set<std::string> claimed;
	std::unordered_set<std::string> seen;
	ScanLines(view, re, "_", [&](const std::cmatch& m) {
		std::string tok = m[0].str();
		std::string base = m[1].str();
		if (!seen.insert(tok).second)
			return;
		if (!declared.count(std::string_view(tok)))
			return;
		if (taken.Has(base) || claimed.count(base))
			return;
		claimed.insert(base);
		renames.emplace_back(std::move(tok), std::move(base));
	});
	ApplyRenames(target, renames);
}

void RenameLoopVars(const std::string& view, std::string& target)
{
	std::vector<std::pair<std::string, std::string>> renames;
	std::unordered_set<std::string> claimed;
	std::unordered_set<std::string> seen;
	const NameSet taken(target);
	static const std::regex kv_re(R"(for\s+(v\d+(?:_\d+)?)\s*,\s*(v\d+(?:_\d+)?)\s+in\s+(i?pairs)\b)");
	ScanLines(view, kv_re, "for", [&](const std::cmatch& m) {
		const bool ip = m[3].length() == 6;
		std::string a = m[1].str();
		std::string b = m[2].str();
		if (seen.insert(a).second)
			renames.emplace_back(std::move(a), TakeName(taken, claimed, ip ? "i" : "key"));
		if (seen.insert(b).second)
			renames.emplace_back(std::move(b), TakeName(taken, claimed, ip ? "item" : "val"));
	});
	static const std::regex it_re(R"(for\s+(v\d+(?:_\d+)?)\s+in\b)");
	ScanLines(view, it_re, "for", [&](const std::cmatch& m) {
		std::string a = m[1].str();
		if (seen.insert(a).second)
			renames.emplace_back(std::move(a), TakeName(taken, claimed, "it"));
	});
	static const std::regex num_re(R"(for\s+(v\d+(?:_\d+)?)\s*=)");
	ScanLines(view, num_re, "for", [&](const std::cmatch& m) {
		std::string a = m[1].str();
		if (seen.insert(a).second)
			renames.emplace_back(std::move(a), TakeName(taken, claimed, "i"));
	});
	ApplyRenames(target, renames);
}

std::string GuessInitName(const std::string& raw)
{
	std::string init = raw;
	const size_t b = init.find_first_not_of(" \t");
	init = (b == std::string::npos) ? std::string() : init.substr(b);
	while (!init.empty() && (init.back() == '\r' || init.back() == ' ' || init.back() == '\t' || init.back() == ';'))
		init.pop_back();
	if (init.empty())
		return "tmp";

	auto leaf_of = [](std::string n) {
		const size_t p = n.find_last_of(".:");
		if (p != std::string::npos)
			n = n.substr(p + 1);
		if (!n.empty())
			n[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(n[0])));
		return n;
	};

	std::smatch m;
	static const std::regex svc(R"(^game:GetService\(\s*["'](\w+)["']\s*\))");
	if (std::regex_search(init, m, svc))
		return m[1].str();
	static const std::regex inew(R"(^Instance\.new\(\s*["'](\w+)["'])");
	if (std::regex_search(init, m, inew))
		return leaf_of(m[1].str());
	if (init.find(":GetChildren()") != std::string::npos)
		return "children";
	if (init.rfind("select(\"#\"", 0) == 0)
		return "argc";
	if (init.rfind("select(", 0) == 0)
		return "first";
	if (init.rfind("coroutine.create", 0) == 0 || init.rfind("coroutine.wrap", 0) == 0)
		return "co";
	if (init.rfind("table.concat", 0) == 0)
		return "joined";
	if (init.rfind("tonumber", 0) == 0)
		return "num";
	if (init.rfind("tostring", 0) == 0)
		return "str";
	if (init.rfind("type(", 0) == 0 || init.rfind("typeof(", 0) == 0)
		return "kind";
	if (init.rfind("math.min", 0) == 0)
		return "min";
	if (init.rfind("math.max", 0) == 0)
		return "max";
	if (init[0] == '#')
		return "count";
	if (init[0] == '"' || init[0] == '\'')
		return "str";
	if (init[0] == '-' || std::isdigit(static_cast<unsigned char>(init[0])))
		return "n";
	if (init == "true" || init == "false" || init.rfind("not ", 0) == 0)
		return "flag";
	if (init[0] == '{')
	{
		static const std::regex wrap(R"(^\{\s*([A-Za-z_][\w.:]*)\s*\()");
		if (std::regex_search(init, m, wrap))
		{
			const std::string n = leaf_of(m[1].str());
			return n.rfind("anon", 0) == 0 ? "res" : n + "_result";
		}
		static const std::regex empt(R"(^\{\s*\}$)");
		if (std::regex_match(init, empt))
			return "t";
		if (init.find('=') != std::string::npos)
			return "map";
		return "arr";
	}
	static const std::regex plain(R"(^[A-Za-z_]\w*$)");
	if (std::regex_match(init, plain))
		return init;
	static const std::regex or_def(R"(^([A-Za-z_]\w*)\s+or\s+)");
	if (std::regex_search(init, m, or_def))
		return m[1].str();
	static const std::regex member(R"(^[A-Za-z_][\w.]*\.([A-Za-z_]\w*)$)");
	if (std::regex_match(init, m, member))
		return m[1].str();
	static const std::regex call(R"(^([A-Za-z_][\w.:]*)\s*\()");
	if (std::regex_search(init, m, call))
	{
		const std::string n = leaf_of(m[1].str());
		return n.rfind("anon", 0) == 0 ? "res" : n + "_result";
	}
	if (init.find("==") != std::string::npos || init.find("~=") != std::string::npos
		|| init.find('<') != std::string::npos || init.find('>') != std::string::npos)
		return "flag";
	return "res";
}

void RenameGenericLocals(const std::string& view, std::string& target)
{
	std::vector<std::pair<std::string, std::string>> renames;
	std::unordered_set<std::string> claimed;
	std::unordered_map<std::string, std::string> done;
	static const std::regex vn_re(R"(^v\d+(?:_\d+)?$)");
	const NameSet taken(target);

	auto claim = [&](std::string old, std::string base) {
		if (done.count(old))
			return;
		if (std::regex_match(base, vn_re))
		{
			auto f = done.find(base);
			base = (f != done.end()) ? f->second : "res";
		}
		std::string n = TakeName(taken, claimed, base);
		renames.emplace_back(old, n);
		done.emplace(std::move(old), std::move(n));
	};

	static const std::regex multi(R"(local\s+(v\d+(?:_\d+)?)\s*,\s*(v\d+(?:_\d+)?)\s*=\s*([^\r\n]+))");
	ScanLines(view, multi, "local", [&](const std::cmatch& m) {
		const std::string base = GuessInitName(m[3].str());
		claim(m[1].str(), base);
		claim(m[2].str(), base);
	});
	static const std::regex single(R"(local\s+(v\d+(?:_\d+)?)\s*=\s*([^\r\n]+))");
	ScanLines(view, single, "local", [&](const std::cmatch& m) {
		claim(m[1].str(), GuessInitName(m[2].str()));
	});
	static const std::regex bare(R"(local\s+(v\d+(?:_\d+)?)[ \t]*\r?\n)");
	ScanLines(view, bare, "local", [&](const std::cmatch& m) { claim(m[1].str(), "tmp"); });

	ApplyRenames(target, renames);
}

// пара соседних строк вместо regex_search по всему тексту с нуля на каждую замену
template <class F>
void FoldLinePairs(std::string& src, const std::regex& re, F repl)
{
	std::string out, prev, pair;
	out.reserve(src.size());
	EachLine(src.data(), src.data() + src.size(), [&](const char* b, const char* e) {
		const size_t len = static_cast<size_t>(e - b);
		if (prev.empty() || prev.size() + len > 2 * k_max_line)
		{
			out += prev;
			prev.assign(b, len);
			return;
		}
		pair.assign(prev);
		pair.append(b, len);
		std::smatch m;
		if (!std::regex_search(pair, m, re))
		{
			out += prev;
			prev.assign(b, len);
			return;
		}
		std::string merged = pair.substr(0, static_cast<size_t>(m.position()));
		merged += repl(m);
		merged.append(pair, static_cast<size_t>(m.position() + m.length()), std::string::npos);
		size_t cut = merged.size();
		if (cut && merged[cut - 1] == '\n')
			--cut;
		const size_t nl = cut ? merged.rfind('\n', cut - 1) : std::string::npos;
		const size_t keep = (nl == std::string::npos) ? 0 : nl + 1;
		out.append(merged, 0, keep);
		prev.assign(merged, keep, std::string::npos);
	});
	out += prev;
	src.swap(out);
}

void InlineSingleUseTemps(std::string& src)
{
	static const std::regex re(
		R"([ \t]*(?:local\s+)?(v\d+)\s*=\s*(\{[^\n]*\})\s*\r?\n[ \t]*([\w.]+)\s*=\s*\{\s*\1\s*\}\s*\r?\n)");
	FoldLinePairs(src, re, [](const std::smatch& m) { return m[3].str() + " = " + m[2].str() + "\n"; });

	static const std::regex re2(
		R"([ \t]*(?:local\s+)?(v\d+)\s*=\s*(\{[^\n]*\})\s*\r?\n[ \t]*([\w.]+)\s*=\s*\{\s*\1\s*,)");
	FoldLinePairs(src, re2, [](const std::smatch& m) { return m[3].str() + " = { " + m[2].str() + ","; });
}

void CleanupEmptyTables(std::string& src)
{
	ReplaceAll(src, "{  }", "{}");
	ReplaceAll(src, "{ }", "{}");
}

void CleanupHeader(std::string& s)
{
	ReplaceAll(s, "Decompiled with the Fission decompiler for RbxCli", "Decompiled with Fission");
	ReplaceAll(s, "InferTypes, ", "");
}

bool IsBlankLine(const char* b, const char* e)
{
	for (; b < e; ++b)
		if (*b != ' ' && *b != '\t' && *b != '\r' && *b != '\n')
			return false;
	return true;
}

std::vector<std::pair<size_t, size_t>> TopLevelSpans(const std::string& src)
{
	std::vector<std::pair<size_t, size_t>> spans;
	size_t seg_start = std::string::npos;
	const char* base = src.data();
	EachLine(base, base + src.size(), [&](const char* b, const char* e) {
		if (*b == ' ' || *b == '\t' || IsBlankLine(b, e))
			return;
		const size_t i = static_cast<size_t>(b - base);
		if (seg_start != std::string::npos)
			spans.emplace_back(seg_start, i);
		seg_start = i;
	});
	if (seg_start != std::string::npos)
		spans.emplace_back(seg_start, src.size());
	return spans;
}

bool IsFunctionSpan(const char* b, const char* e)
{
	static const std::regex re(R"(^(local\s+)?function\s+[A-Za-z_])");
	if (static_cast<size_t>(e - b) > 64)
		e = b + 64;
	return std::regex_search(b, e, re);
}

struct Block { size_t start, end; bool is_function; };

std::vector<Block> BuildBlocks(const std::string& src)
{
	std::vector<Block> blocks;
	for (const auto& [s, e] : TopLevelSpans(src))
	{
		const bool isfn = IsFunctionSpan(src.data() + s, src.data() + e);
		if (!blocks.empty() && blocks.back().is_function == isfn && blocks.back().end == s)
			blocks.back().end = e;
		else
			blocks.push_back({ s, e, isfn });
	}
	return blocks;
}

std::string ChunkView(const std::string& src)
{
	std::string view;
	view.reserve(src.size());
	for (const auto& b : BuildBlocks(src))
	{
		if (!b.is_function)
			view.append(src, b.start, b.end - b.start);
	}
	return view;
}

void BeautifyScope(const std::string& view, std::string& target)
{
	StripFissionSuffixes(view, target);
	BeautifyPcallsSimple(view, target);
	RenameFromAssignments(view, target);
	RenameLoopVars(view, target);
	RenameGenericLocals(view, target);
}

void BeautifyDecompiled(std::string& src)
{
	if (src.size() > k_max_beautify)
	{
		CleanupHeader(src);
		src.insert(0, "-- beautify skipped: " + std::to_string(src.size()) + " bytes > "
			+ std::to_string(k_max_beautify) + " limit, raw decompiler output below\n");
		return;
	}

	StripTypeAnnotations(src);
	InlineSingleUseTemps(src);
	RemoveDeadNilLocals(src);

	std::string out;
	out.reserve(src.size());
	for (const auto& b : BuildBlocks(src))
	{
		std::string block = src.substr(b.start, b.end - b.start);
		if (b.is_function)
			BeautifyScope(block, block);
		out += block;
	}
	src.swap(out);

	StripFissionSuffixes(ChunkView(src), src);
	BeautifyPcallsSimple(ChunkView(src), src);
	RenameFromAssignments(ChunkView(src), src);
	RenameAnimTables(ChunkView(src), src);
	RenameLoopVars(ChunkView(src), src);
	RenameGenericLocals(ChunkView(src), src);

	RemovePcallKills(src);
	CleanupEmptyTables(src);
	CleanupHeader(src);
}

} // namespace

std::string DecompileLuauBytecode(const std::uint8_t* data, std::size_t n)
{
	if (!data || n == 0)
		return {};

	EnableLuauFFlags();

	const std::string bc(reinterpret_cast<const char*>(data), n);
	Decompiler dec{};
	const auto flags = DecompilerFlags::AutoNameVariables
		| DecompilerFlags::OmitFissionComments
		| DecompilerFlags::OptimizeIR;

	DecompilationResult r{};
	try
	{
		CoutMute mute{};
		r = dec.DecompileRobloxBytecode(bc, flags);
	}
	catch (...)
	{
		return {};
	}
	if (r.resultCode != DecompileResult::Success)
		return {};

	std::string out = std::move(r.decompilationOutput);
	if (out.size() < 8)
		return {};

	bool beautified = true;
	try
	{
		UnescapeLuaByteEscapes(out);
		BeautifyDecompiled(out);
	}
	catch (...)
	{
		beautified = false;
	}
	// проходы меняют строку только через swap в самом конце, так что out остался целым
	if (!beautified)
		out.insert(0, "-- beautify aborted (exception), output is only partially processed\n");
	return out;
}

void UnescapeLuaByteEscapes(std::string& src)
{
	std::string out;
	out.reserve(src.size());

	bool in_str = false;
	char quote = 0;

	for (size_t i = 0; i < src.size();)
	{
		const char c = src[i];

		if (!in_str)
		{
			if (c == '"' || c == '\'')
			{
				in_str = true;
				quote = c;
			}
			out.push_back(c);
			++i;
			continue;
		}

		if (c == quote)
		{
			in_str = false;
			quote = 0;
			out.push_back(c);
			++i;
			continue;
		}

		if (c != '\\' || i + 1 >= src.size())
		{
			out.push_back(c);
			++i;
			continue;
		}

		const char n1 = src[i + 1];

		// fission: deserializer пишет \208, generator ещё раз \\ → \\208
		if (n1 == '\\')
		{
			int val = 0;
			size_t end = 0;
			if (TryReadDecByte(src, i + 2, val, end))
			{
				out.push_back(static_cast<char>(val));
				i = end;
				continue;
			}
		}

		// \xHH
		if (n1 == 'x' || n1 == 'X')
		{
			if (i + 3 < src.size())
			{
				auto hex = [](char h) -> int {
					if (h >= '0' && h <= '9') return h - '0';
					if (h >= 'a' && h <= 'f') return h - 'a' + 10;
					if (h >= 'A' && h <= 'F') return h - 'A' + 10;
					return -1;
				};
				const int hi = hex(src[i + 2]);
				const int lo = hex(src[i + 3]);
				if (hi >= 0 && lo >= 0)
				{
					out.push_back(static_cast<char>((hi << 4) | lo));
					i += 4;
					continue;
				}
			}
		}

		// \ddd single
		if (n1 >= '0' && n1 <= '9')
		{
			int val = 0;
			size_t end = 0;
			if (TryReadDecByte(src, i + 1, val, end))
			{
				out.push_back(static_cast<char>(val));
				i = end;
				continue;
			}
		}

		// \n \t \\ \" — как есть
		out.push_back(c);
		out.push_back(n1);
		i += 2;
	}

	src.swap(out);
}

} // namespace FissionEmbed
} // namespace Features
} // namespace Cheat
