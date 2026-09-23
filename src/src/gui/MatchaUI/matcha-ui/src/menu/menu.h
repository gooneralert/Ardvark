#pragma once

#include <D3D11.h>

class Menu
{
public:
	static bool Initialize(HWND hWnd, ID3D11Device* pDevice, ID3D11DeviceContext* pDeviceContext);
	static void Shutdown();

public:
	static void Render();
	static bool HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static void InvalidateDeviceObjects();
	static void CreateDeviceObjects();
	static void DrawMenu();
	static void DrawWatermark();
	static void DrawKeybinds();

	inline static bool m_bMenuVisible   = true;
	inline static bool m_bStartupPhase  = false;
	inline static bool m_bShowLauncher  = false;
	inline static int  m_iLauncherMode  = 1;
	inline static bool m_bShowLoading   = false;
	inline static bool m_bOpenMenuAfterLoading = false;
	inline static float m_fLoadingStartTime = 0.f;

	// Fade-in / fade-out
	// Set m_bClosing = true from render.cpp when the menu key is pressed to close.
	// DrawMenu() will drive alphas to 0 and render.cpp must keep calling DrawMenu()
	// until m_fMenuAlpha == 0, then stop and reset m_bClosing.
	inline static bool  m_bClosing      = false;
	inline static float m_fMenuAlpha    = 0.f;   // 0 = invisible, 1 = fully visible
	inline static float m_fPreviewAlpha = 0.f;   // ESP preview, delayed behind menu

	// D3D11 device stored at init for texture creation
	inline static ID3D11Device*        s_d3d_device  = nullptr;
	inline static ID3D11DeviceContext* s_d3d_context = nullptr;

private:
	inline static bool m_bInitialized      = false;
	inline static float m_fLoadingProgress = 0.f;
};
