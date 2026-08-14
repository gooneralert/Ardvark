#pragma once

#include "../State.h"
#include "jewsploit/colors/colors.h"
#include "gui/resources/fonts/fonts.h"
#include "gui/TextEditor/TextEditor.h"
#include "imgui.h"

#include <memory>

namespace Cheat {
namespace Features {
namespace LuaDetail {

inline float PanelFs()
{
	return fonts::ui_size(fonts::ui_bold());
}

inline ImU32 ToU32(const ImVec4& c)
{
	return ImGui::ColorConvertFloat4ToU32(c);
}

inline ImU32 MixU32(const ImVec4& a, const ImVec4& b, float t, float alpha = 1.0f)
{
	ImVec4 m;
	m.x = a.x + (b.x - a.x) * t;
	m.y = a.y + (b.y - a.y) * t;
	m.z = a.z + (b.z - a.z) * t;
	m.w = alpha;
	return ToU32(m);
}

inline TextEditor::Palette MakePalette()
{
	TextEditor::Palette p = TextEditor::GetDarkPalette();

	col::theme_t& th = col::live();
	ImVec4 bg = ImVec4(th.child[0], th.child[1], th.child[2], 1.f);
	ImVec4 text = ImVec4(0.92f, 0.93f, 0.96f, 1.f);
	ImVec4 dim = ImVec4(0.55f, 0.58f, 0.64f, 1.f);
	ImVec4 acc = ImVec4(th.accent[0], th.accent[1], th.accent[2], 1.f);

	p[(int)TextEditor::PaletteIndex::Default] = ToU32(text);
	p[(int)TextEditor::PaletteIndex::Keyword] = ToU32(acc);
	p[(int)TextEditor::PaletteIndex::Number] = MixU32(acc, ImVec4(0.55f, 0.95f, 0.70f, 1.f), 0.55f);
	p[(int)TextEditor::PaletteIndex::String] = MixU32(acc, ImVec4(0.95f, 0.75f, 0.45f, 1.f), 0.65f);
	p[(int)TextEditor::PaletteIndex::CharLiteral] = MixU32(acc, ImVec4(0.95f, 0.75f, 0.45f, 1.f), 0.45f);
	p[(int)TextEditor::PaletteIndex::Punctuation] = MixU32(text, dim, 0.25f);
	p[(int)TextEditor::PaletteIndex::Preprocessor] = MixU32(acc, ImVec4(0.70f, 0.55f, 0.95f, 1.f), 0.50f);
	p[(int)TextEditor::PaletteIndex::Identifier] = ToU32(text);
	p[(int)TextEditor::PaletteIndex::KnownIdentifier] = MixU32(acc, text, 0.35f);
	p[(int)TextEditor::PaletteIndex::PreprocIdentifier] = MixU32(acc, ImVec4(0.85f, 0.55f, 0.90f, 1.f), 0.40f);
	p[(int)TextEditor::PaletteIndex::Comment] = ToU32(ImVec4(dim.x, dim.y, dim.z, 0.85f));
	p[(int)TextEditor::PaletteIndex::MultiLineComment] = ToU32(ImVec4(dim.x, dim.y, dim.z, 0.75f));
	p[(int)TextEditor::PaletteIndex::Background] = ToU32(bg);
	p[(int)TextEditor::PaletteIndex::Cursor] = ToU32(text);
	p[(int)TextEditor::PaletteIndex::Selection] = ToU32(ImVec4(acc.x, acc.y, acc.z, 0.35f));
	p[(int)TextEditor::PaletteIndex::ErrorMarker] = IM_COL32(255, 60, 60, 140);
	p[(int)TextEditor::PaletteIndex::Breakpoint] = IM_COL32(255, 140, 40, 90);
	p[(int)TextEditor::PaletteIndex::LineNumber] = ToU32(ImVec4(dim.x, dim.y, dim.z, 0.70f));
	p[(int)TextEditor::PaletteIndex::CurrentLineFill] = ToU32(ImVec4(acc.x, acc.y, acc.z, 0.08f));
	p[(int)TextEditor::PaletteIndex::CurrentLineFillInactive] = ToU32(ImVec4(1.f, 1.f, 1.f, 0.03f));
	p[(int)TextEditor::PaletteIndex::CurrentLineEdge] = ToU32(ImVec4(acc.x, acc.y, acc.z, 0.35f));
	return p;
}

inline void ConfigureEditor(TextEditor& ed)
{
	ed.SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());
	ed.SetPalette(MakePalette());
	ed.SetShowWhitespaces(false);
	ed.SetTabSize(4);
	ed.SetHandleKeyboardInputs(true);
	ed.SetHandleMouseInputs(true);
}

inline std::unique_ptr<TextEditor> MakeEditor(const char* seed = "")
{
	auto ed = std::make_unique<TextEditor>();
	ConfigureEditor(*ed);
	if (seed && seed[0])
		ed->SetText(seed);
	return ed;
}

}
}
}
