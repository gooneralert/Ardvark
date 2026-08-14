#pragma once

#include "Tabs.h"
#include "../protocol/Log.h"

#include <Windows.h>
#include <shellapi.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace Cheat {
namespace Features {
namespace LuaDetail {

inline fs::path ScriptsDir()
{
	wchar_t mod[MAX_PATH]{};
	GetModuleFileNameW(nullptr, mod, MAX_PATH);
	fs::path dir = fs::path(mod).parent_path() / "scripts";
	std::error_code ec;
	fs::create_directories(dir, ec);
	return dir;
}

inline void RefreshScripts()
{
	g_scripts.clear();
	std::error_code ec;
	for (auto& e : fs::directory_iterator(ScriptsDir(), ec))
	{
		if (!e.is_regular_file(ec))
			continue;

		auto ext = e.path().extension().string();
		if (_stricmp(ext.c_str(), ".lua") != 0 && _stricmp(ext.c_str(), ".txt") != 0)
			continue;

		g_scripts.push_back(e.path().filename().string());
	}
	std::sort(g_scripts.begin(), g_scripts.end());
}

inline bool LoadFileIntoTab(const fs::path& path)
{
	std::ifstream in(path, std::ios::binary);
	if (!in)
	{
		PushOutput(LogLevel::Error, ("failed to open " + path.filename().string()).c_str());
		return false;
	}

	std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	// 256kb хватит
	if (data.size() > 256 * 1024)
		data.resize(256 * 1024);

	for (int i = 0; i < (int)g_tabs.size(); ++i)
	{
		if (g_tabs[i].path == path.string())
		{
			g_tab = i;
			if (!g_tabs[i].editor)
				g_tabs[i].text = data;
			g_tabs[i].editor = MakeEditor();
			ConfigureEditor(*g_tabs[i].editor);
			g_tabs[i].editor->SetText(data);
			PushOutput(LogLevel::Info, ("opened " + path.filename().string()).c_str());
			return true;
		}
	}

	EditorTab t;
	t.name = path.stem().string();
	t.path = path.string();
	t.text = data;
	t.editor = MakeEditor();
	t.editor->SetText(data);
	g_tabs.push_back(std::move(t));
	g_tab = (int)g_tabs.size() - 1;
	PushOutput(LogLevel::Info, ("opened " + path.filename().string()).c_str());
	return true;
}

inline bool SaveCurrent()
{
	auto& t = Cur();

	fs::path path;
	if (t.path.empty())
		path = ScriptsDir() / (t.name + ".lua");

	else
		path = fs::path(t.path);

	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	if (!out)
		return false;

	out.write(t.text.data(), (std::streamsize)t.text.size());
	t.path = path.string();
	t.name = path.stem().string();
	RefreshScripts();
	return true;
}

inline void OpenScriptsFolder()
{
	ShellExecuteW(nullptr, L"open", ScriptsDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

}
}
}
