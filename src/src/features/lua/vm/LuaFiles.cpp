#include "pch.h"
#include "LuaFiles.h"
#include "LuaPreprocess.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace Cheat {
namespace Features {
namespace LuaFiles {
namespace {

std::string WorkspaceRoot()
{
	// Ardvark workspace folder (created automatically).
	fs::path p;
	char* buf = nullptr;
	size_t len = 0;
	if (_dupenv_s(&buf, &len, "LOCALAPPDATA") == 0 && buf && *buf)
		p = fs::path(buf) / "Ardvark" / "workspace";
	free(buf);
	if (p.empty())
		p = fs::current_path() / "Ardvark" / "workspace";
	std::error_code ec;
	fs::create_directories(p, ec);
	return p.string();
}

// is p on/under root, normalized + case-insensitive?
bool IsUnder(const fs::path& p, const fs::path& root)
{
	std::error_code ec;
	std::string t = fs::weakly_canonical(p, ec).string();
	std::string r = fs::weakly_canonical(root, ec).string();
	if (r.empty())
		r = root.lexically_normal().string();
	if (r.empty())
		return false;
	for (char& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	for (char& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return t.size() >= r.size() && t.compare(0, r.size(), r) == 0;
}

// Redirect every filesystem path into the Ardvark workspace folder.
// C:\matcha[...] and relative paths are remapped under the workspace root.
fs::path Resolve(const std::string& path)
{
	const fs::path ws(WorkspaceRoot());
	std::string s = path;
	std::string low = s;
	for (char& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

	static const std::string k_prefixes[] = {
		"c:\\matcha\\workspace", "c:/matcha/workspace",
		"c:\\matcha", "c:/matcha",
	};
	for (const auto& p : k_prefixes)
	{
		if (low.compare(0, p.size(), p) == 0)
		{
			s.erase(0, p.size());
			break;
		}
	}
	while (!s.empty() && (s[0] == '/' || s[0] == '\\'))
		s.erase(0, 1);

	fs::path target = ws / s;
	std::error_code ec;
	fs::path norm = fs::weakly_canonical(target, ec);
	if (ec || norm.empty())
		norm = target.lexically_normal();

	if (!IsUnder(norm, ws))
		return fs::path();
	return norm;
}

int l_listfiles(lua_State* L)
{
	const char* path = luaL_optstring(L, 1, "");
	fs::path target = Resolve(path ? path : "");

	lua_newtable(L);
	if (target.empty() || !fs::exists(target) || !fs::is_directory(target))
		return 1;

	int i = 1;
	std::error_code ec;
	for (const auto& entry : fs::directory_iterator(target, ec))
	{
		const std::string name = entry.path().filename().string();
		lua_pushlstring(L, name.data(), name.size());
		lua_rawseti(L, -2, i++);
	}
	return 1;
}

int l_isfile(lua_State* L)
{
	const char* path = luaL_checkstring(L, 1);
	fs::path target = Resolve(path ? path : "");
	lua_pushboolean(L, !target.empty() && fs::is_regular_file(target) ? 1 : 0);
	return 1;
}

int l_isfolder(lua_State* L)
{
	const char* path = luaL_checkstring(L, 1);
	fs::path target = Resolve(path ? path : "");
	lua_pushboolean(L, !target.empty() && fs::is_directory(target) ? 1 : 0);
	return 1;
}

int l_writefile(lua_State* L)
{
	const char* path = luaL_checkstring(L, 1);
	size_t len = 0;
	const char* data = luaL_checklstring(L, 2, &len);

	const char* err = nullptr;
	{
		fs::path target = Resolve(path ? path : "");
		if (target.empty())
			err = "invalid path";
		else if (fs::is_directory(target))
			err = "path is a folder";
		else
		{
			std::error_code ec;
			fs::create_directories(target.parent_path(), ec);
			std::ofstream file(target, std::ios::binary | std::ios::trunc);
			if (file)
				file.write(data, static_cast<std::streamsize>(len));
		}
	}
	if (err)
		return luaL_error(L, "writefile: %s", err);
	return 0;
}

int l_readfile(lua_State* L)
{
	const char* path = luaL_checkstring(L, 1);
	const char* err = nullptr;
	{
		fs::path target = Resolve(path ? path : "");
		if (target.empty() || !fs::is_regular_file(target))
		{
			err = "file not found";
		}
		else
		{
			std::ifstream file(target, std::ios::binary);
			if (file)
			{
				std::stringstream buffer;
				buffer << file.rdbuf();
				const std::string s = buffer.str();
				lua_pushlstring(L, s.data(), s.size());
			}
			else
			{
				err = "cannot open";
			}
		}
	}
	if (err)
	{
		// non-fatal: return nil instead of raising, so guards like
		// `pcall(readfile, path)` and `isfile` still work without a console error
		lua_pushnil(L);
		return 1;
	}
	return 1;
}

int l_appendfile(lua_State* L)
{
	const char* path = luaL_checkstring(L, 1);
	size_t len = 0;
	const char* data = luaL_checklstring(L, 2, &len);

	const char* err = nullptr;
	{
		fs::path target = Resolve(path ? path : "");
		if (target.empty())
			err = "invalid path";
		else if (fs::is_directory(target))
			err = "path is a folder";
		else
		{
			std::error_code ec;
			fs::create_directories(target.parent_path(), ec);
			std::ofstream file(target, std::ios::binary | std::ios::app);
			if (file)
				file.write(data, static_cast<std::streamsize>(len));
		}
	}
	if (err)
		return luaL_error(L, "appendfile: %s", err);
	return 0;
}

int l_loadfile(lua_State* L)
{
	const char* path = luaL_checkstring(L, 1);
	fs::path target = Resolve(path ? path : "");
	if (target.empty() || !fs::is_regular_file(target))
	{
		lua_pushnil(L);
		lua_pushstring(L, "loadfile: file not found");
		return 2;
	}

	std::ifstream file(target, std::ios::binary);
	if (!file)
	{
		lua_pushnil(L);
		lua_pushstring(L, "loadfile: cannot open");
		return 2;
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	const std::string s = buffer.str();

	const std::string processed = LuaPreprocess::Preprocess(s);
	if (luaL_loadbuffer(L, processed.data(), processed.size(), path ? path : "file") != LUA_OK)
		return 2; // nil, err already pushed
	return 1;
}

int l_delfile(lua_State* L)
{
	const char* path = luaL_checkstring(L, 1);
	fs::path target = Resolve(path ? path : "");
	if (target.empty() || !fs::is_regular_file(target))
		return 0;
	std::error_code ec;
	fs::remove(target, ec);
	return 0;
}

int l_makefolder(lua_State* L)
{
	const char* path = luaL_checkstring(L, 1);
	fs::path target = Resolve(path ? path : "");
	if (target.empty())
		return 0;
	std::error_code ec;
	fs::create_directories(target, ec);
	return 0;
}

int l_delfolder(lua_State* L)
{
	const char* path = luaL_checkstring(L, 1);
	fs::path target = Resolve(path ? path : "");
	if (target.empty() || !fs::is_directory(target))
		return 0;
	std::error_code ec;
	fs::remove_all(target, ec);
	return 0;
}

void Set(lua_State* L, const char* name, lua_CFunction fn)
{
	lua_pushcfunction(L, fn);
	lua_setglobal(L, name);
}

} // namespace

void Register(lua_State* L)
{
	Set(L, "writefile",   l_writefile);
	Set(L, "readfile",    l_readfile);
	Set(L, "appendfile",  l_appendfile);
	Set(L, "makefolder",  l_makefolder);
	Set(L, "isfile",      l_isfile);
	Set(L, "isfolder",    l_isfolder);
	Set(L, "listfiles",   l_listfiles);
	Set(L, "delfile",     l_delfile);
	Set(L, "delfolder",   l_delfolder);
	Set(L, "loadfile",    l_loadfile);
}

} // namespace LuaFiles
} // namespace Features
} // namespace Cheat