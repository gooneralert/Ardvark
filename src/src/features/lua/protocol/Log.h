#pragma once

#include "../State.h"
#include "jewsploit/colors/colors.h"
#include "imgui.h"

namespace Cheat {
namespace Features {
namespace LuaDetail {

inline const char* LevelTag(LogLevel level)
{
	switch (level)
	{
	case LogLevel::Print:   return "print";
	case LogLevel::Warn:    return "warn";
	case LogLevel::Error:   return "error";
	case LogLevel::Success: return "ok";
	default:                return "info";
	}
}

inline ImU32 LevelColor(LogLevel level)
{
	switch (level)
	{
	case LogLevel::Print:
		return IM_COL32(230, 235, 245, 255);
	case LogLevel::Warn:
		return IM_COL32(230, 180, 70, 255);
	case LogLevel::Error:
		return IM_COL32(235, 85, 85, 255);
	case LogLevel::Success:
		return col::accent_u32(255);
	default:
		return IM_COL32(140, 145, 155, 255);
	}
}

inline void PushOutput(LogLevel level, const char* text)
{
	if (!text)
		return;

	OutputLine line;
	line.level = level;
	line.text = text;

	{
		std::lock_guard<std::mutex> lock(g_output_mu);
		g_output.push_back(std::move(line));

		if (g_output.size() > 2000)
			g_output.erase(g_output.begin(), g_output.begin() + (g_output.size() - 2000));
	}

	if (g_out_auto_scroll)
		g_scroll_out = true;
}

}
}
}
