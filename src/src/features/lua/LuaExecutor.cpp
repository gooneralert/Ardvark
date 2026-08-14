#include "pch.h"
#define NOMINMAX
#define IMGUI_DEFINE_MATH_OPERATORS
#include "LuaExecutor.h"
#include "vm/LuaVM.h"

#include "execute/Tabs.h"
#include "execute/Scripts.h"
#include "protocol/Log.h"
#include "app/Settings.h"
#include "gui/lua_window.h"

#include <cstdarg>
#include <cstdio>

namespace Cheat::Features {

void LuaExecutor::Initialize()
{
	if (LuaDetail::g_inited)
		return;

	LuaDetail::g_inited = true;
	LuaDetail::EnsureDefaultTab();
	LuaDetail::RefreshScripts();
	LuaVM::Initialize();
}

void LuaExecutor::Shutdown()
{
	LuaVM::Shutdown();
	LuaDetail::g_tabs.clear();
	LuaDetail::g_scripts.clear();
	LuaDetail::g_output.clear();
	LuaDetail::g_tab = 0;
	LuaDetail::g_out_sel = -1;
	LuaDetail::g_inited = false;
}

void LuaExecutor::ClearOutput()
{
	LuaDetail::g_output.clear();
	LuaDetail::g_out_sel = -1;
}

void LuaExecutor::Log(LogLevel level, const char* fmt, ...)
{
	if (!fmt)
		return;

	char buf[2048];
	va_list args;
	va_start(args, fmt);
	std::vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	LuaDetail::PushOutput(level, buf);
}

void LuaExecutor::Render(float alpha)
{
	(void)alpha;
	gui::render_lua_window(&g_Settings.lua.executor);
}

}
