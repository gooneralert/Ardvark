#include "pch.h"
#include "Explorer.h"

#include "gui/explorer_window.h"
#include "gui/icons.h"
#include "app/Graphics.h"
#include "app/Settings.h"

namespace Cheat::Features {

void Explorer::Initialize()
{
	gui::icons_init(Cheat::Core::g_Device);
	gui::explorer_init();
}

void Explorer::Shutdown()
{
	gui::explorer_shutdown();
}

void Explorer::Render(float alpha)
{
	if (alpha <= 0.001f)
		return;
	if (!Cheat::g_Settings.misc.explorer)
		return;
	gui::render_explorer_window(&Cheat::g_Settings.misc.explorer);
}

}
