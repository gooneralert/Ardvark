#pragma once
#include <windows.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct ID3D11RenderTargetView;

namespace Cheat {

    class Renderer {
    public:
        static bool Initialize(HINSTANCE instance);
        static void Shutdown();
        static void MainLoop();

        static bool IsGameActive() { return m_GameActive; }

        static HWND GetHwnd() { return m_Hwnd; }
        static HWND GetGameHwnd() { return m_GameHwnd; }
        static void SetClickThrough(bool click_through);
        // оверлей без активации, дернуть когда imgui хочет клаву
        static void SetTextInputFocus(bool want_text);

    private:
        static bool CreateDevice();
        static void CleanupDevice();
        static void CreateRenderTarget();
        static void CleanupRenderTarget();
        static LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

        static HWND FindGameWindow();
        static void SyncToGameWindow();
        static void ResizeSwapchain(int w, int h);

        inline static HWND m_Hwnd = nullptr;
        inline static HWND m_GameHwnd = nullptr;
        inline static bool m_GameActive = false;
        inline static int m_Width = 0;
        inline static int m_Height = 0;
        inline static DWORD m_LastEnumTick = 0;
        inline static DWORD m_CachedEnumPid = 0;

        inline static ID3D11Device* m_Device = nullptr;
        inline static ID3D11DeviceContext* m_DeviceContext = nullptr;
        inline static IDXGISwapChain* m_SwapChain = nullptr;
        inline static ID3D11RenderTargetView* m_RenderTargetView = nullptr;
    };

}
