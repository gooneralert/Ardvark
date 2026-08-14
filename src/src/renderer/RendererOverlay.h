#pragma once
// только из Renderer.cpp

#include "Renderer.h"
#include "gui/Menu.h"
#include "imgui.h"
#include "app/Graphics.h"
#include "core/memory/Memory.h"
#include "core/console/Console.h"
#include "features/visuals/boxfill/BoxFill.h"
#include "features/visuals/mesh/MeshDxShader.h"
#include <dwmapi.h>
#include <windowsx.h>

namespace Cheat {

	// окно роблокса по пиду, берём самое жирное
	HWND Renderer::FindGameWindow()
	{
		DWORD pid = g_Memory.GetPID();
		if (!pid)
		{
			return nullptr;
		}

		struct Ctx
		{
			DWORD pid;
			HWND best = nullptr;
			int best_area = 0;
		} ctx{ pid };

		EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL
		{
			auto* c = (Ctx*)lp;
			DWORD wpid = 0;
			GetWindowThreadProcessId(hwnd, &wpid);
			if (wpid != c->pid)
			{
				return TRUE;
			}

			if (!IsWindowVisible(hwnd) || IsIconic(hwnd))
			{
				return TRUE;
			}

			if (GetWindow(hwnd, GW_OWNER) != nullptr)
			{
				return TRUE;
			}

			RECT cr{};
			if (!GetClientRect(hwnd, &cr))
			{
				return TRUE;
			}

			int w = cr.right - cr.left;
			int h = cr.bottom - cr.top;
			if (w < 200 || h < 200)
			{
				return TRUE;
			}

			wchar_t title[256]{};
			GetWindowTextW(hwnd, title, 256);
			if (title[0] == L'\0')
			{
				return TRUE;
			}

			int area = w * h;
			if (area > c->best_area)
			{
				c->best_area = area;
				c->best = hwnd;
			}
			return TRUE;
		}, (LPARAM)&ctx);

		return ctx.best;
	}

	// клеим оверлей к клиентской области
	void Renderer::SyncToGameWindow()
	{
		DWORD pid = g_Memory.GetPID();
		DWORD now = GetTickCount();

		// раз в 300мс пересканируем, иначе держим кэш
		bool need_enum =
			!m_GameHwnd ||
			!IsWindow(m_GameHwnd) ||
			pid != m_CachedEnumPid ||
			(now - m_LastEnumTick) >= 300;

		if (need_enum)
		{
			m_GameHwnd = FindGameWindow();
			m_LastEnumTick = now;
			m_CachedEnumPid = pid;
		}

		m_GameActive = false;

		if (!m_GameHwnd || !IsWindow(m_GameHwnd) || IsIconic(m_GameHwnd))
		{
			if (IsWindowVisible(m_Hwnd))
			{
				ShowWindow(m_Hwnd, SW_HIDE);
			}
			return;
		}

		HWND fg = GetForegroundWindow();
		// меню открыто ок тока если фокус на игре или на оверлее
		// иначе Win+Shift+S / чужое окно — гуй в скрине
		bool focused =
			(fg == m_GameHwnd) ||
			IsChild(m_GameHwnd, fg) ||
			(fg == m_Hwnd);

		if (!focused)
		{
			if (IsWindowVisible(m_Hwnd))
			{
				ShowWindow(m_Hwnd, SW_HIDE);
			}
			return;
		}

		RECT cr{};
		if (!GetClientRect(m_GameHwnd, &cr))
		{
			if (IsWindowVisible(m_Hwnd))
			{
				ShowWindow(m_Hwnd, SW_HIDE);
			}
			return;
		}

		POINT tl = { cr.left, cr.top };
		POINT br = { cr.right, cr.bottom };
		ClientToScreen(m_GameHwnd, &tl);
		ClientToScreen(m_GameHwnd, &br);

		int w = br.x - tl.x;
		int h = br.y - tl.y;
		if (w < 64 || h < 64)
		{
			if (IsWindowVisible(m_Hwnd))
			{
				ShowWindow(m_Hwnd, SW_HIDE);
			}
			return;
		}

		ResizeSwapchain(w, h);

		SetWindowPos(
			m_Hwnd, HWND_TOPMOST,
			tl.x, tl.y, w, h,
			SWP_SHOWWINDOW | SWP_NOACTIVATE);

		m_GameActive = true;
	}

	bool Renderer::Initialize(HINSTANCE instance)
	{
		WNDCLASSEXW wc = {
			sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, instance,
			nullptr, nullptr, nullptr, nullptr, L"jewsploit.overlay", nullptr
		};
		if (!RegisterClassExW(&wc))
		{
			Console::Log(Console::Color::Red, "overlay fail window class");
			return false;
		}

		m_Hwnd = CreateWindowExW(
			WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED |
			WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
			wc.lpszClassName,
			L"",
			WS_POPUP,
			0, 0, 2, 2,
			nullptr, nullptr, wc.hInstance, nullptr);
		if (!m_Hwnd)
		{
			Console::Log(Console::Color::Red, "overlay fail window");
			return false;
		}

		SetLayeredWindowAttributes(m_Hwnd, 0, 255, LWA_ALPHA);

		MARGINS margins = { -1 };
		DwmExtendFrameIntoClientArea(m_Hwnd, &margins);

		if (!CreateDevice())
		{
			Console::Log(Console::Color::Red, "overlay fail dx11");
			CleanupDevice();
			UnregisterClassW(wc.lpszClassName, wc.hInstance);
			return false;
		}

		ShowWindow(m_Hwnd, SW_HIDE);

		Cheat::Core::g_Device = m_Device;
		Cheat::Core::g_DeviceContext = m_DeviceContext;
		Cheat::Core::g_SwapChain = m_SwapChain;
		Cheat::Core::g_RenderTargetView = m_RenderTargetView;

		Cheat::Visuals::BoxFill::Init(m_Device);
		Visuals::MeshDxShader::Init(m_Device, m_DeviceContext);

		if (!GUI::Menu::Initialize(m_Hwnd, m_Device, m_DeviceContext))
		{
			Console::Log(Console::Color::Red, "overlay fail menu");
			CleanupDevice();
			UnregisterClassW(wc.lpszClassName, wc.hInstance);
			return false;
		}

		return true;
	}

	void Renderer::SetClickThrough(bool click_through)
	{
		if (!m_Hwnd || !IsWindow(m_Hwnd))
		{
			return;
		}

		bool want_text = ImGui::GetCurrentContext() && ImGui::GetIO().WantTextInput;
		LONG want;
		if (click_through)
		{
			want = (LONG)(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED |
				WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE);
		}

		else
		{
			want = (LONG)(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE);
		}

		// без NOACTIVATE текст в imgui не фокусится
		if (want_text && !click_through)
		{
			want &= ~WS_EX_NOACTIVATE;
		}

		LONG cur = GetWindowLong(m_Hwnd, GWL_EXSTYLE);
		if (cur == want)
		{
			return;
		}

		SetWindowLong(m_Hwnd, GWL_EXSTYLE, want);
		SetWindowPos(
			m_Hwnd, nullptr, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER |
			SWP_FRAMECHANGED | SWP_NOACTIVATE);
	}

	void Renderer::SetTextInputFocus(bool want_text)
	{
		if (!m_Hwnd || !IsWindow(m_Hwnd))
		{
			return;
		}

		static bool s_had_text = false;
		LONG style = GetWindowLong(m_Hwnd, GWL_EXSTYLE);

		if (want_text)
		{
			style &= ~(WS_EX_NOACTIVATE | WS_EX_TRANSPARENT);
			SetWindowLong(m_Hwnd, GWL_EXSTYLE, style);
			SetWindowPos(
				m_Hwnd, HWND_TOPMOST, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
			if (GetFocus() != m_Hwnd)
			{
				SetFocus(m_Hwnd);
			}
			s_had_text = true;
		}

		else if (s_had_text)
		{
			style |= WS_EX_NOACTIVATE;
			SetWindowLong(m_Hwnd, GWL_EXSTYLE, style);
			SetWindowPos(
				m_Hwnd, nullptr, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER |
				SWP_FRAMECHANGED | SWP_NOACTIVATE);
			if (m_GameHwnd && IsWindow(m_GameHwnd))
			{
				SetForegroundWindow(m_GameHwnd);
			}
			s_had_text = false;
		}
	}

	void Renderer::Shutdown()
	{
		GUI::Menu::Shutdown();
		Cheat::Visuals::BoxFill::Shutdown();
		Visuals::MeshDxShader::Shutdown();

		CleanupDevice();
		Cheat::Core::g_Device = nullptr;
		Cheat::Core::g_DeviceContext = nullptr;
		Cheat::Core::g_SwapChain = nullptr;
		Cheat::Core::g_RenderTargetView = nullptr;

		if (m_Hwnd)
		{
			DestroyWindow(m_Hwnd);
			m_Hwnd = nullptr;
		}
	}

	LRESULT WINAPI Renderer::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		if (msg == WM_NCHITTEST)
		{
			if (GUI::Menu::IsVisible())
			{
				return HTCLIENT;
			}
			return HTTRANSPARENT;
		}

		if (GUI::Menu::HandleMessage(hwnd, msg, wparam, lparam))
		{
			return true;
		}

		switch (msg)
		{
		case WM_SIZE:
			if (m_Device != nullptr && wparam != SIZE_MINIMIZED)
			{
				int w = (int)LOWORD(lparam);
				int h = (int)HIWORD(lparam);
				if (w > 0 && h > 0)
				{
					ResizeSwapchain(w, h);
				}
			}
			return 0;

		case WM_MOUSEACTIVATE:
			if (GUI::Menu::IsVisible() && ImGui::GetCurrentContext() &&
				ImGui::GetIO().WantTextInput)
			{
				return MA_ACTIVATE;
			}
			return MA_NOACTIVATE;

		case WM_SYSCOMMAND:
			if ((wparam & 0xfff0) == SC_KEYMENU)
			{
				return 0;
			}
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		}
		return DefWindowProcW(hwnd, msg, wparam, lparam);
	}

}
