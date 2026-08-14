#pragma once

#include "LuaExecutor.h"
#include "gui/TextEditor/TextEditor.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Cheat {
namespace Features {
namespace LuaDetail {

using LogLevel = LuaExecutor::LogLevel;

struct EditorTab {
	std::string name;
	std::string path;
	std::string text;
	std::unique_ptr<TextEditor> editor;
};

struct OutputLine {
	LogLevel level = LogLevel::Info;
	std::string text;
};

inline std::vector<EditorTab> g_tabs;
inline int g_tab = 0;
inline std::vector<std::string> g_scripts;
inline std::vector<OutputLine> g_output;
inline std::mutex g_output_mu;
inline bool g_scroll_out = false;
inline float g_refresh_at = 0.f;
inline bool g_inited = false;

// ui
inline float g_editor_frac = 0.62f;   // editor share of left column
inline bool g_out_auto_scroll = true;
inline bool g_out_wrap = true;
inline int g_out_sel = -1;
inline int g_out_sel_end = -1; // range copy, -1 = одна строка
inline int g_out_filter_mode = 0; // 0=all 1=print 2=warn 3=error 4=ok/info
inline char g_out_filter[96] = {};
inline char g_find_buf[96] = {};

}
}
}
