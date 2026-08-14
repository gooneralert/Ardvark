#pragma once

#include <Windows.h>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace Cheat {
namespace GUI {

    class Menu
    {
    public:
        static bool Initialize(HWND hWnd, ID3D11Device* pDevice, ID3D11DeviceContext* pDeviceContext);
        static void Shutdown();

        static void Render();
        static bool HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

        static void InvalidateDeviceObjects();
        static void CreateDeviceObjects();

        static bool IsVisible();
        static bool IsPointOverUI(float x, float y);
        static bool ShouldCaptureMouse(float x, float y);

    private:
        static float DrawMenu();

        inline static bool  m_bInitialized = false;
        inline static bool  m_bMenuVisible = false;
        inline static float m_menuAlpha = 1.0f;
    };

}
}
