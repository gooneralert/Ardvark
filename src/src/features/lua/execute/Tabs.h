#pragma once

#include "../protocol/Editor.h"
#include "../protocol/Log.h"
#include "Clipboard.h"

#include <cstdio>
#include <memory>
#include <string>

namespace Cheat {
namespace Features {
namespace LuaDetail {

inline void EnsureDefaultTab()
{
	if (!g_tabs.empty())
		return;

	EditorTab t;
	t.name = "main";
	t.text = "-- jewsploit lua\nprint(\"hello\")\n";
	t.editor = MakeEditor(t.text.c_str());
	g_tabs.push_back(std::move(t));
	g_tab = 0;
}

inline EditorTab& Cur()
{
	EnsureDefaultTab();
	if (g_tab < 0 || g_tab >= (int)g_tabs.size())
		g_tab = 0;
	return g_tabs[g_tab];
}

inline std::string& CurText()
{
	return Cur().text;
}

inline TextEditor& CurEditor()
{
	auto& t = Cur();
	if (!t.editor)
		t.editor = MakeEditor(t.text.c_str());
	return *t.editor;
}

inline void NewBlankTab()
{
	EditorTab t;
	int n = 1;
	for (;;)
	{
		char name[32];
		std::snprintf(name, sizeof(name), "tab %d", n);
		bool used = false;
		for (const auto& e : g_tabs)
		{
			if (e.name == name)
			{
				used = true;
				break;
			}
		}

		if (!used)
		{
			t.name = name;
			break;
		}

		++n;
	}

	t.text.clear();
	t.editor = MakeEditor();
	g_tabs.push_back(std::move(t));
	g_tab = (int)g_tabs.size() - 1;
}

inline void CloseTab(int idx)
{
	if (idx < 0 || idx >= (int)g_tabs.size())
		return;

	// последний таб не убиваем, просто чистим
	if (g_tabs.size() == 1)
	{
		auto& t = g_tabs[0];
		t.text.clear();
		t.editor = MakeEditor();
		t.path.clear();
		t.name = "main";
		g_tab = 0;
		return;
	}

	g_tabs.erase(g_tabs.begin() + idx);
	if (g_tab >= (int)g_tabs.size())
		g_tab = (int)g_tabs.size() - 1;

	else if (g_tab > idx)
		--g_tab;
}

inline void CopyCurrent()
{
	SetClipboardText(CurText().c_str());
}

inline void PasteIntoCurrent()
{
	const std::string clip = GetClipboardText();
	if (clip.empty())
		return;
	CurText() += clip;
}

inline void ClearCurrent()
{
	CurText().clear();
}

}
}
}
