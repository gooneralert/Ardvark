#pragma once
// только из Renderer.cpp

#include "Renderer.h"
#include <d3d11.h>
#include "gui/Menu.h"
#include "imgui.h"
#include "features/visuals/crosshair/Crosshair.h"

namespace Cheat {

	void Renderer::MainLoop()
	{
		bool running = true;
		while (running)
		{
			MSG msg;
			while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				if (msg.message == WM_QUIT)
				{
					running = false;
				}
			}
			if (!running)
			{
				break;
			}

			SyncToGameWindow();

			if (!m_GameActive)
			{
				Visuals::Crosshair::NotifyInactive();
				Sleep(15);
				continue;
			}

			float clear[4] = { 0.f, 0.f, 0.f, 0.f };
			m_DeviceContext->OMSetRenderTargets(1, &m_RenderTargetView, nullptr);
			m_DeviceContext->ClearRenderTargetView(m_RenderTargetView, clear);

			GUI::Menu::Render();

			// клик-тру / фокус под imgui
			{
				POINT pt{};
				GetCursorPos(&pt);
				ScreenToClient(m_Hwnd, &pt);
				bool over_ui = GUI::Menu::ShouldCaptureMouse((float)pt.x, (float)pt.y);

				if (GUI::Menu::IsVisible() || over_ui)
				{
					SetClickThrough(!over_ui);
					if (GUI::Menu::IsVisible())
					{
						ClipCursor(nullptr);
					}

					bool want_text = GUI::Menu::IsVisible() &&
						ImGui::GetCurrentContext() && ImGui::GetIO().WantTextInput;
					SetTextInputFocus(want_text);
				}

				else
				{
					SetClickThrough(true);
					SetTextInputFocus(false);
				}
			}

			// 0 = без vsync оверлея: меньше задержка относительно камеры игры
			m_SwapChain->Present(0, 0);
		}

		Visuals::Crosshair::Shutdown();
	}

}
