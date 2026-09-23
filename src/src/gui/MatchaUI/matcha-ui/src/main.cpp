#include <windows.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <tchar.h>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_dx11.h>
#include <imgui/backends/imgui_impl_win32.h>

#include "menu/menu.h"
#include "menu/keybind/keybind.h"
#include "render/render.h"
#include "render/backdrop_blur.h"
#include "settings.h"
#include "stubs.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (Menu::HandleMessage(hWnd, msg, wParam, lParam))
		return true;
	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcA(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	matcha_ui_init_stubs();

	WNDCLASSEXA wc{};
	wc.cbSize = sizeof(wc);
	wc.style = CS_CLASSDC;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = GetModuleHandleA(nullptr);
	wc.lpszClassName = "MatchaUI";
	RegisterClassExA(&wc);

	const int sw = GetSystemMetrics(SM_CXSCREEN);
	const int sh = GetSystemMetrics(SM_CYSCREEN);
	HWND hwnd = CreateWindowExA(
		WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
		wc.lpszClassName, "Matcha UI",
		WS_POPUP, 0, 0, sw, sh,
		nullptr, nullptr, wc.hInstance, nullptr);
	if (!hwnd)
		return 1;

	SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);
	MARGINS margins{ -1, -1, -1, -1 };
	DwmExtendFrameIntoClientArea(hwnd, &margins);
	ShowWindow(hwnd, SW_SHOW);

	DXGI_SWAP_CHAIN_DESC sd{};
	sd.BufferCount = 1;
	sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	sd.OutputWindow = hwnd;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Windowed = TRUE;
	sd.SampleDesc.Count = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

	D3D_FEATURE_LEVEL level{};
	D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
	if (FAILED(D3D11CreateDeviceAndSwapChain(
		nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
		levels, 2, D3D11_SDK_VERSION, &sd,
		&render->detail->swap_chain, &render->detail->device, &level, &render->detail->device_context)))
		return 1;

	render->detail->window = hwnd;
	ID3D11Texture2D* back = nullptr;
	render->detail->swap_chain->GetBuffer(0, IID_PPV_ARGS(&back));
	render->detail->device->CreateRenderTargetView(back, nullptr, &render->detail->render_target_view);
	if (back) back->Release();

	if (!Menu::Initialize(hwnd, render->detail->device, render->detail->device_context))
		return 1;
	backdrop_blur::init(render->detail->device, render->detail->device_context);

	Menu::m_bClosing = false;
	Menu::m_fMenuAlpha = 0.f;
	render->running = true;

	MSG msg{};
	while (msg.message != WM_QUIT)
	{
		while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
			if (msg.message == WM_QUIT)
				break;
		}
		if (msg.message == WM_QUIT)
			break;

		if ((GetAsyncKeyState(VK_INSERT) & 1) != 0)
		{
			if (Menu::m_bClosing || Menu::m_fMenuAlpha <= 0.f)
			{
				Menu::m_bClosing = false;
				if (Menu::m_fMenuAlpha <= 0.f)
					Menu::m_fMenuAlpha = 0.f;
			}
			else
			{
				Menu::m_bClosing = true;
			}
		}

		keybind::begin_frame();
		backdrop_blur::update();

		const float clear[4] = { 0, 0, 0, 0 };
		render->detail->device_context->OMSetRenderTargets(1, &render->detail->render_target_view, nullptr);
		render->detail->device_context->ClearRenderTargetView(render->detail->render_target_view, clear);
		Menu::Render();
		render->detail->swap_chain->Present(settings::menu::vsync ? 1 : 0, 0);
	}

	backdrop_blur::shutdown();
	Menu::Shutdown();
	if (render->detail->render_target_view) render->detail->render_target_view->Release();
	if (render->detail->swap_chain) render->detail->swap_chain->Release();
	if (render->detail->device_context) render->detail->device_context->Release();
	if (render->detail->device) render->detail->device->Release();
	DestroyWindow(hwnd);
	UnregisterClassA(wc.lpszClassName, wc.hInstance);
	return 0;
}
