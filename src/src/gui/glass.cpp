// -----------------------------------------------------------------------------
// glass.cpp - DirectComposition acrylic backdrop for the menu window.
//
// VERBATIM port of the Win32-Acrylic-Effect reference (AcrylicCompositor):
// a dedicated popup window gets DWM shared-thumbnail visuals of the desktop and
// of the top-level windows behind it, run through a gaussian blur + saturation
// effect graph composed live by DWM. The blur radius and tint are driven by the
// settings sliders.
// -----------------------------------------------------------------------------
#include "pch.h"
#include "glass.h"
#include "renderer/Renderer.h"
#include "core/console/Console.h"
#include <Windows.h>
#include <atomic>
#include <memory>
#include <vector>
#include <algorithm>
#include <cstdarg>
#include <cstdio>

#include <dcomp.h>
#include <d2d1_2.h>
#include <d2d1_2helper.h>
#include <dxgi1_3.h>
#include <d3d11_2.h>
#include <dwmapi.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <winternl.h>

#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d2d1")
#pragma comment(lib, "dcomp")
#pragma comment(lib, "dwmapi")

using namespace Microsoft::WRL;

// private DWM thumbnail flags (from the reference header)
#define DWM_TNP_FREEZE            0x100000
#define DWM_TNP_ENABLE3D          0x4000000
#define DWM_TNP_DISABLE3D         0x8000000
#define DWM_TNP_FORCECVI          0x40000000
#define DWM_TNP_DISABLEFORCECVI   0x80000000

namespace glass
{
    namespace
    {
        void GLog(const char* fmt, ...)
        {
            char buf[512];
            va_list ap; va_start(ap, fmt);
            _vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
            va_end(ap);
            Cheat::Console::Log(Cheat::Console::Color::Gray, "[glass] %s", buf);
        }

        // =====================================================================
        // AcrylicCompositor - verbatim port of the reference class.
        // =====================================================================
        class AcrylicCompositor
        {
        public:
            enum BackdropSource
            {
                BACKDROP_SOURCE_DESKTOP = 0x0,
                BACKDROP_SOURCE_HOSTBACKDROP = 0x1
            };

            struct AcrylicEffectParameter
            {
                float blurAmount;
                float saturationAmount;
                D2D1_COLOR_F tintColor;
                D2D1_COLOR_F fallbackColor;
            };

            AcrylicCompositor(HWND hwnd)
            {
                InitLibs();
                CreateCompositionDevice();
                CreateEffectGraph(dcompDevice3.Get());
            }

            bool SetAcrylicEffect(HWND hwnd, BackdropSource source, AcrylicEffectParameter params)
            {
                fallbackColor = params.fallbackColor;
                tintColor = params.tintColor;
                if (source == BACKDROP_SOURCE_HOSTBACKDROP)
                {
                    BOOL enable = TRUE;
                    WINDOWCOMPOSITIONATTRIBDATA CompositionAttribute{};
                    CompositionAttribute.Attrib = WCA_EXCLUDED_FROM_LIVEPREVIEW;
                    CompositionAttribute.pvData = &enable;
                    CompositionAttribute.cbData = sizeof(BOOL);
                    DwmSetWindowCompositionAttribute(hwnd, &CompositionAttribute);
                }

                CreateBackdrop(hwnd, source);
                CreateCompositionVisual(hwnd);
                CreateFallbackVisual();
                fallbackVisual->SetContent(swapChain.Get());
                rootVisual->RemoveAllVisuals();
                switch (source)
                {
                    case BACKDROP_SOURCE_DESKTOP:
                        rootVisual->AddVisual(desktopWindowVisual.Get(), false, NULL);
                        rootVisual->AddVisual(fallbackVisual.Get(), true, desktopWindowVisual.Get());
                        break;
                    case BACKDROP_SOURCE_HOSTBACKDROP:
                        rootVisual->AddVisual(desktopWindowVisual.Get(), false, NULL);
                        rootVisual->AddVisual(topLevelWindowVisual.Get(), true, desktopWindowVisual.Get());
                        rootVisual->AddVisual(fallbackVisual.Get(), true, topLevelWindowVisual.Get());
                        break;
                    default:
                        rootVisual->RemoveAllVisuals();
                        break;
                }

                rootVisual->SetClip(clip.Get());
                rootVisual->SetTransform(translateTransform.Get());

                saturationEffect->SetSaturation(params.saturationAmount);

                blurEffect->SetBorderMode(D2D1_BORDER_MODE_HARD);
                blurEffect->SetInput(0, saturationEffect.Get(), 0);
                blurEffect->SetStandardDeviation(params.blurAmount);

                rootVisual->SetEffect(blurEffect.Get());
                Commit();

                SyncCoordinates(hwnd);

                return true;
            }

            // live blur control (slider)
            void SetBlurAmount(float sigma)
            {
                if (blurEffect)
                {
                    blurEffect->SetStandardDeviation(sigma);
                    Commit();
                }
            }

            // re-sync clip/transform after the window moved while hidden
            void SyncCoordinates2(HWND hwnd) { SyncCoordinates(hwnd); }

            // live tint control (frost slider)
            void SetTintColor(D2D1_COLOR_F color, bool active)
            {
                tintColor = color;
                SyncFallbackVisual(active);
            }

            long GetBuildVersion()
            {
                if (GetVersionInfo != nullptr)
                {
                    RTL_OSVERSIONINFOW versionInfo = { 0 };
                    versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
                    if (GetVersionInfo(&versionInfo) == 0x00000000)
                    {
                        return versionInfo.dwBuildNumber;
                    }
                }
                return 0;
            }

        public:
            enum WINDOWCOMPOSITIONATTRIB
            {
                WCA_EXCLUDED_FROM_LIVEPREVIEW = 0xD,
                WCA_ACCENT_POLICY = 0x13,
            };

            struct WINDOWCOMPOSITIONATTRIBDATA
            {
                WINDOWCOMPOSITIONATTRIB Attrib;
                void* pvData;
                DWORD cbData;
            };

            ComPtr<ID2D1Device1> d2Device;
            ComPtr<ID3D11Device> d3d11Device;
            ComPtr<IDXGIDevice2> dxgiDevice;
            ComPtr<IDXGIFactory2> dxgiFactory;
            ComPtr<ID2D1Factory2> d2dFactory2;
            ComPtr<ID2D1DeviceContext> deviceContext;
            ComPtr<IDCompositionDesktopDevice> dcompDevice;
            ComPtr<IDCompositionDevice3> dcompDevice3;
            ComPtr<IDCompositionTarget> dcompTarget;

            ComPtr<IDCompositionVisual2> rootVisual;
            ComPtr<IDCompositionVisual2> fallbackVisual;
            ComPtr<IDCompositionVisual2> desktopWindowVisual;
            ComPtr<IDCompositionVisual2> topLevelWindowVisual;

            // acrylic essentials
            ComPtr<IDCompositionGaussianBlurEffect> blurEffect;
            ComPtr<IDCompositionSaturationEffect> saturationEffect;
            ComPtr<IDCompositionTranslateTransform> translateTransform;
            ComPtr<IDCompositionRectangleClip> clip;

            // fallback visual
            DXGI_SWAP_CHAIN_DESC1 description = {};
            D2D1_BITMAP_PROPERTIES1 properties = {};
            ComPtr<IDXGISwapChain1> swapChain;
            ComPtr<IDXGISurface2> fallbackSurface;
            ComPtr<ID2D1Bitmap1> fallbackBitmap;
            ComPtr<ID2D1SolidColorBrush> fallbackBrush;
            D2D1_COLOR_F tintColor = D2D1::ColorF(0.055f, 0.055f, 0.063f, 0.45f);
            D2D1_COLOR_F fallbackColor = D2D1::ColorF(0.10f, 0.10f, 0.10f, 1.0f);
            D2D1_RECT_F fallbackRect = D2D1::RectF(0, 0, (float)GetSystemMetrics(SM_CXSCREEN), (float)GetSystemMetrics(SM_CYSCREEN));

            // desktop backdrop
            HWND desktopWindow;
            RECT desktopWindowRect;
            SIZE thumbnailSize{};
            DWM_THUMBNAIL_PROPERTIES thumbnail;
            HTHUMBNAIL desktopThumbnail = NULL;

            // top level window backdrop
            RECT sourceRect{ 0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN) };
            SIZE destinationSize{ GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN) };
            HTHUMBNAIL topLevelWindowThumbnail = NULL;
            HWND* hwndExclusionList;

            typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
            typedef BOOL(WINAPI* SetWindowCompositionAttribute)(IN HWND hwnd, IN WINDOWCOMPOSITIONATTRIBDATA* pwcad);
            typedef HRESULT(WINAPI* DwmpCreateSharedThumbnailVisual)(IN HWND hwndDestination, IN HWND hwndSource, IN DWORD dwThumbnailFlags, IN DWM_THUMBNAIL_PROPERTIES* pThumbnailProperties, IN VOID* pDCompDevice, OUT VOID** ppVisual, OUT PHTHUMBNAIL phThumbnailId);
            typedef HRESULT(WINAPI* DwmpCreateSharedMultiWindowVisual)(IN HWND hwndDestination, IN VOID* pDCompDevice, OUT VOID** ppVisual, OUT PHTHUMBNAIL phThumbnailId);
            typedef HRESULT(WINAPI* DwmpUpdateSharedMultiWindowVisual)(IN HTHUMBNAIL hThumbnailId, IN HWND* phwndsInclude, IN DWORD chwndsInclude, IN HWND* phwndsExclude, IN DWORD chwndsExclude, OUT RECT* prcSource, OUT SIZE* pDestinationSize, IN DWORD dwFlags);
            typedef HRESULT(WINAPI* DwmpCreateSharedVirtualDesktopVisual)(IN HWND hwndDestination, IN VOID* pDCompDevice, OUT VOID** ppVisual, OUT PHTHUMBNAIL phThumbnailId);
            typedef HRESULT(WINAPI* DwmpUpdateSharedVirtualDesktopVisual)(IN HTHUMBNAIL hThumbnailId, IN HWND* phwndsInclude, IN DWORD chwndsInclude, IN HWND* phwndsExclude, IN DWORD chwndsExclude, OUT RECT* prcSource, OUT SIZE* pDestinationSize);

            DwmpCreateSharedThumbnailVisual DwmCreateSharedThumbnailVisual;
            DwmpCreateSharedMultiWindowVisual DwmCreateSharedMultiWindowVisual;
            DwmpUpdateSharedMultiWindowVisual DwmUpdateSharedMultiWindowVisual;
            DwmpCreateSharedVirtualDesktopVisual DwmCreateSharedVirtualDesktopVisual;
            DwmpUpdateSharedVirtualDesktopVisual DwmUpdateSharedVirtualDesktopVisual;
            SetWindowCompositionAttribute DwmSetWindowCompositionAttribute;
            RtlGetVersionPtr GetVersionInfo;

            HRESULT hr;
            RECT hostWindowRect{};

            bool InitLibs()
            {
                auto dwmapi = LoadLibrary(L"dwmapi.dll");
                auto user32 = LoadLibrary(L"user32.dll");
                auto ntdll = GetModuleHandleW(L"ntdll.dll");

                if (!dwmapi || !user32 || !ntdll)
                {
                    return false;
                }

                GetVersionInfo = (RtlGetVersionPtr)GetProcAddress(ntdll, "RtlGetVersion");
                DwmSetWindowCompositionAttribute = (SetWindowCompositionAttribute)GetProcAddress(user32, "SetWindowCompositionAttribute");
                DwmCreateSharedThumbnailVisual = (DwmpCreateSharedThumbnailVisual)GetProcAddress(dwmapi, MAKEINTRESOURCEA(147));
                DwmCreateSharedMultiWindowVisual = (DwmpCreateSharedMultiWindowVisual)GetProcAddress(dwmapi, MAKEINTRESOURCEA(163));
                DwmUpdateSharedMultiWindowVisual = (DwmpUpdateSharedMultiWindowVisual)GetProcAddress(dwmapi, MAKEINTRESOURCEA(164));
                DwmCreateSharedVirtualDesktopVisual = (DwmpCreateSharedVirtualDesktopVisual)GetProcAddress(dwmapi, MAKEINTRESOURCEA(163));
                DwmUpdateSharedVirtualDesktopVisual = (DwmpUpdateSharedVirtualDesktopVisual)GetProcAddress(dwmapi, MAKEINTRESOURCEA(164));

                return true;
            }

            bool CreateCompositionDevice()
            {
                if (D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0, D3D11_SDK_VERSION, d3d11Device.GetAddressOf(), nullptr, nullptr) != S_OK)
                {
                    GLog("D3D11CreateDevice failed");
                    return false;
                }

                if (d3d11Device->QueryInterface(dxgiDevice.GetAddressOf()) != S_OK)
                {
                    GLog("QI IDXGIDevice2 failed");
                    return false;
                }

                if (D2D1CreateFactory(D2D1_FACTORY_TYPE::D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory2), (void**)d2dFactory2.GetAddressOf()) != S_OK)
                {
                    GLog("D2D1CreateFactory failed");
                    return false;
                }

                if (d2dFactory2->CreateDevice(dxgiDevice.Get(), d2Device.GetAddressOf()) != S_OK)
                {
                    GLog("D2D CreateDevice failed");
                    return false;
                }

                if (DCompositionCreateDevice3(dxgiDevice.Get(), __uuidof(dcompDevice), (void**)dcompDevice.GetAddressOf()) != S_OK)
                {
                    GLog("DCompositionCreateDevice3 failed");
                    return false;
                }

                if (dcompDevice->QueryInterface(__uuidof(IDCompositionDevice3), (LPVOID*)&dcompDevice3) != S_OK)
                {
                    GLog("QI IDCompositionDevice3 failed");
                    return false;
                }

                GLog("DComp device ready");
                return true;
            }

            bool CreateFallbackVisual()
            {
                description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
                description.BufferCount = 2;
                description.SampleDesc.Count = 1;
                description.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

                description.Width = GetSystemMetrics(SM_CXSCREEN);
                description.Height = GetSystemMetrics(SM_CYSCREEN);

                d3d11Device.As(&dxgiDevice);

                if (CreateDXGIFactory2(0, __uuidof(dxgiFactory), reinterpret_cast<void**>(dxgiFactory.GetAddressOf())) != S_OK)
                {
                    GLog("CreateDXGIFactory2 failed");
                    return false;
                }

                if (dxgiFactory->CreateSwapChainForComposition(dxgiDevice.Get(), &description, nullptr, swapChain.GetAddressOf()) != S_OK)
                {
                    GLog("CreateSwapChainForComposition failed");
                    return false;
                }

                if (d2Device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, deviceContext.GetAddressOf()) != S_OK)
                {
                    GLog("CreateDeviceContext failed");
                    return false;
                }

                if (swapChain->GetBuffer(0, __uuidof(fallbackSurface), reinterpret_cast<void**>(fallbackSurface.GetAddressOf())) != S_OK)
                {
                    GLog("GetBuffer failed");
                    return false;
                }

                properties.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
                properties.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
                properties.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
                if (deviceContext->CreateBitmapFromDxgiSurface(fallbackSurface.Get(), properties, fallbackBitmap.GetAddressOf()) != S_OK)
                {
                    GLog("CreateBitmapFromDxgiSurface failed");
                    return false;
                }

                deviceContext->SetTarget(fallbackBitmap.Get());
                deviceContext->BeginDraw();
                deviceContext->Clear();
                deviceContext->CreateSolidColorBrush(tintColor, fallbackBrush.GetAddressOf());
                deviceContext->FillRectangle(fallbackRect, fallbackBrush.Get());
                deviceContext->EndDraw();

                if (swapChain->Present(1, 0) != S_OK)
                {
                    GLog("Present failed");
                    return false;
                }

                return true;
            }

            bool CreateCompositionVisual(HWND hwnd)
            {
                dcompDevice3->CreateVisual(&rootVisual);
                dcompDevice3->CreateVisual(&fallbackVisual);

                if (!CreateCompositionTarget(hwnd))
                {
                    return false;
                }

                if (dcompTarget->SetRoot(rootVisual.Get()) != S_OK)
                {
                    return false;
                }

                return true;
            }

            bool CreateCompositionTarget(HWND hwnd)
            {
                if (dcompDevice->CreateTargetForHwnd(hwnd, FALSE, dcompTarget.GetAddressOf()) != S_OK)
                {
                    GLog("CreateTargetForHwnd failed");
                    return false;
                }

                return true;
            }

            bool CreateBackdrop(HWND hwnd, BackdropSource source)
            {
                switch (source)
                {
                    case BACKDROP_SOURCE_DESKTOP:
                        desktopWindow = (HWND)FindWindow(L"Progman", NULL);

                        GetWindowRect(desktopWindow, &desktopWindowRect);
                        thumbnailSize.cx = (desktopWindowRect.right - desktopWindowRect.left);
                        thumbnailSize.cy = (desktopWindowRect.bottom - desktopWindowRect.top);

                        thumbnail.dwFlags = DWM_TNP_SOURCECLIENTAREAONLY | DWM_TNP_VISIBLE | DWM_TNP_RECTDESTINATION | DWM_TNP_RECTSOURCE | DWM_TNP_OPACITY | DWM_TNP_ENABLE3D;
                        thumbnail.opacity = 255;
                        thumbnail.fVisible = TRUE;
                        thumbnail.fSourceClientAreaOnly = FALSE;
                        thumbnail.rcDestination = RECT{ 0, 0, thumbnailSize.cx, thumbnailSize.cy };
                        thumbnail.rcSource = RECT{ 0, 0, thumbnailSize.cx, thumbnailSize.cy };
                        if (DwmCreateSharedThumbnailVisual(hwnd, desktopWindow, 2, &thumbnail, dcompDevice.Get(), (void**)desktopWindowVisual.GetAddressOf(), &desktopThumbnail) != S_OK)
                        {
                            GLog("CreateSharedThumbnailVisual(147) failed");
                            return false;
                        }
                        break;
                    case BACKDROP_SOURCE_HOSTBACKDROP:
                        if (GetBuildVersion() >= 20000)
                        {
                            hr = DwmCreateSharedMultiWindowVisual(hwnd, dcompDevice.Get(), (void**)topLevelWindowVisual.GetAddressOf(), &topLevelWindowThumbnail);
                        }
                        else
                        {
                            hr = DwmCreateSharedVirtualDesktopVisual(hwnd, dcompDevice.Get(), (void**)topLevelWindowVisual.GetAddressOf(), &topLevelWindowThumbnail);
                        }

                        if (hr != S_OK)
                        {
                            GLog("CreateSharedMultiWindowVisual(163) failed hr=0x%08X", (unsigned)hr);
                            return false;
                        }

                        if (!CreateBackdrop(hwnd, BACKDROP_SOURCE_DESKTOP) || hr != S_OK)
                        {
                            return false;
                        }
                        hwndExclusionList = new HWND[1];
                        hwndExclusionList[0] = (HWND)0x0;

                        if (GetBuildVersion() >= 20000)
                        {
                            hr = DwmUpdateSharedMultiWindowVisual(topLevelWindowThumbnail, NULL, 0, hwndExclusionList, 1, &sourceRect, &destinationSize, 1);
                        }
                        else
                        {
                            hr = DwmUpdateSharedVirtualDesktopVisual(topLevelWindowThumbnail, NULL, 0, hwndExclusionList, 1, &sourceRect, &destinationSize);
                        }

                        if (hr != S_OK)
                        {
                            GLog("UpdateSharedMultiWindowVisual(164) failed hr=0x%08X", (unsigned)hr);
                            return false;
                        }
                        break;
                }
                return true;
            }

            bool CreateEffectGraph(ComPtr<IDCompositionDevice3> dcompDevice3)
            {
                if (dcompDevice3->CreateGaussianBlurEffect(blurEffect.GetAddressOf()) != S_OK)
                {
                    return false;
                }
                if (dcompDevice3->CreateSaturationEffect(saturationEffect.GetAddressOf()) != S_OK)
                {
                    return false;
                }
                if (dcompDevice3->CreateTranslateTransform(&translateTransform) != S_OK)
                {
                    return false;
                }
                if (dcompDevice3->CreateRectangleClip(&clip) != S_OK)
                {
                    return false;
                }
                return true;
            }

            void SyncCoordinates(HWND hwnd)
            {
                GetWindowRect(hwnd, &hostWindowRect);
                clip->SetLeft((float)hostWindowRect.left);
                clip->SetRight((float)hostWindowRect.right);
                clip->SetTop((float)hostWindowRect.top);
                clip->SetBottom((float)hostWindowRect.bottom);
                rootVisual->SetClip(clip.Get());
                // WS_POPUP has no non-client frame, so the offset is exactly the
                // window origin (the reference subtracted frame metrics for its
                // WS_OVERLAPPEDWINDOW).
                translateTransform->SetOffsetX(-1 * (float)hostWindowRect.left);
                translateTransform->SetOffsetY(-1 * (float)hostWindowRect.top);
                rootVisual->SetTransform(translateTransform.Get());
                // NOTE: no DwmFlush here - it blocks until vblank and was capping
                // drag FPS. The DComp Commit is applied at the next composition
                // pass anyway, which is the same deadline.
                Commit();
            }

            bool Sync(HWND hwnd, int msg, WPARAM wParam, LPARAM lParam, bool active)
            {
                switch (msg)
                {
                    case WM_ACTIVATE:
                        SyncFallbackVisual(active);
                        Flush();
                        return true;
                    case WM_WINDOWPOSCHANGED:
                        SyncCoordinates(hwnd);
                        return true;
                    case WM_CLOSE:
                        delete[] hwndExclusionList;
                        return true;
                }
                return false;
            }

            bool Flush()
            {
                if (topLevelWindowThumbnail != NULL)
                {
                    if (GetBuildVersion() >= 20000)
                    {
                        DwmUpdateSharedMultiWindowVisual(topLevelWindowThumbnail, NULL, 0, hwndExclusionList, 1, &sourceRect, &destinationSize, 1);
                    }
                    else
                    {
                        DwmUpdateSharedVirtualDesktopVisual(topLevelWindowThumbnail, NULL, 0, hwndExclusionList, 1, &sourceRect, &destinationSize);
                    }
                    DwmFlush();
                }
                return true;
            }

            bool Commit()
            {
                if (dcompDevice->Commit() != S_OK)
                {
                    return false;
                }
                return true;
            }

            void SyncFallbackVisual(bool active)
            {
                if (!active)
                {
                    fallbackBrush->SetColor(fallbackColor);
                }
                else
                {
                    fallbackBrush->SetColor(tintColor);
                }

                deviceContext->BeginDraw();
                deviceContext->Clear();
                deviceContext->FillRectangle(fallbackRect, fallbackBrush.Get());
                deviceContext->EndDraw();
                swapChain->Present(1, 0);
            }
        };

        // =====================================================================
        // integration layer
        // =====================================================================
        std::unique_ptr<AcrylicCompositor> g_compositor;
        std::atomic<float> g_frost{ 0.5f };
        std::atomic<float> g_blur{ 10.f };
        HWND   g_acrylic   = nullptr;
        HWND   g_overlay   = nullptr;
        bool   g_registered = false;
        bool   g_dcomp_ok  = false;
        bool   g_accent_fb = false;
        RECT   g_lastRect{};
        std::vector<RECT>  g_rects;
        std::vector<float> g_rounds;
        std::vector<RECT>  g_lastNorm;
        bool   g_haveLast  = false;
        DWORD  g_lastFlush = 0;
        RECT   g_lastScreenRect = {};  // popup pos in SCREEN coords
        bool   g_haveScreen = false;

        struct ACCENTPOLICY { int nAccentState; int nFlags; int nColor; int nAnimationId; };
        struct WINCOMPATTRDATA { int nAttribute; void* pData; uint32_t ulSizeOfData; };

        static bool SetWindowComposition(HWND hWnd, int nAccentState)
        {
            const auto lib = LoadLibraryW(L"user32.dll");
            if (!lib) return false;
            typedef BOOL(WINAPI* pSetWindowCompositionAttribute)(HWND, WINCOMPATTRDATA*);
            const auto fn = (pSetWindowCompositionAttribute)GetProcAddress(lib, "SetWindowCompositionAttribute");
            if (!fn) { FreeLibrary(lib); return false; }
            ACCENTPOLICY policy{ nAccentState, 2, 0x99000000, 0 };
            WINCOMPATTRDATA data{ 19, &policy, sizeof(ACCENTPOLICY) };
            const bool ok = fn(hWnd, &data) != FALSE;
            FreeLibrary(lib);
            return ok;
        }

        // rounded corners for the backdrop window (matches the menu border radius)
        static void UpdateRgn(HWND hwnd)
        {
            RECT r{};
            if (!GetWindowRect(hwnd, &r)) return;
            const int w = r.right - r.left, h = r.bottom - r.top;
            HRGN rgn = CreateRoundRectRgn(0, 0, w + 1, h + 1, 16, 16);
            SetWindowRgn(hwnd, rgn, FALSE);
        }

        static LRESULT CALLBACK AcrylicWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            if (g_compositor)
                g_compositor->Sync(hwnd, msg, wParam, lParam, true);
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        static bool CreateAcrylicWindow()
        {
            if (g_acrylic) return true;
            HINSTANCE hInst = GetModuleHandleW(nullptr);
            if (!g_registered)
            {
                WNDCLASSW wc = {};
                wc.lpfnWndProc = AcrylicWndProc;
                wc.hInstance = hInst;
                wc.lpszClassName = L"jewsploit_acrylic";
                wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
                if (!RegisterClassW(&wc)) return false;
                g_registered = true;
            }
            // exactly like the reference window: WS_EX_NOREDIRECTIONBITMAP so DWM
            // composites the DComp visual directly. NOACTIVATE+TOOLWINDOW keep it
            // out of the taskbar and focus chain; it must NOT be layered.
            g_acrylic = CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                L"jewsploit_acrylic", L"", WS_POPUP,
                0, 0, 100, 100, nullptr, nullptr, hInst, nullptr);
            if (!g_acrylic) return false;

            g_overlay = FindWindowW(L"jewsploit_overlay", nullptr);
            // sit directly below the main overlay in z-order so the menu draws on top
            if (g_overlay)
                SetWindowPos(g_acrylic, g_overlay, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            return true;
        }

        static void ApplyAccentFallback()
        {
            if (g_accent_fb || !g_acrylic) return;
            if (SetWindowComposition(g_acrylic, 4)) // ACCENT_ENABLE_ACRYLICBLURBEHIND
            {
                g_accent_fb = true;
                Cheat::Console::Log(Cheat::Console::Color::Yellow, "[glass] DirectComposition unavailable, using accent fallback");
            }
        }

        static void AttemptInit()
        {
            if (g_compositor || g_accent_fb) return;
            Cheat::Console::Log(Cheat::Console::Color::Cyan, "[glass] trying DirectComposition acrylic");

            g_compositor.reset(new AcrylicCompositor(g_acrylic));

            AcrylicCompositor::AcrylicEffectParameter param;
            param.blurAmount = g_blur.load();
            param.saturationAmount = 1.0f;
            const float fa = std::min(0.85f, 0.12f + 0.55f * g_frost.load());
            param.tintColor = D2D1::ColorF(0.055f, 0.055f, 0.063f, fa);
            param.fallbackColor = D2D1::ColorF(0.10f, 0.10f, 0.10f, 1.0f);

            if (g_compositor->SetAcrylicEffect(g_acrylic, AcrylicCompositor::BACKDROP_SOURCE_HOSTBACKDROP, param))
            {
                g_dcomp_ok = true;
                Cheat::Console::Log(Cheat::Console::Color::Green, "[glass] DirectComposition acrylic ACTIVE (blur adjustable)");
            }
            else
            {
                g_compositor.reset();
                ApplyAccentFallback();
            }
        }

        float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
    }

    void init(ID3D11Device* device, ID3D11DeviceContext* context)
    {
        (void)device; (void)context;
    }

    void shutdown()
    {
        if (g_acrylic)
        {
            DestroyWindow(g_acrylic);
            g_acrylic = nullptr;
        }
        g_compositor.reset();
        g_dcomp_ok = g_accent_fb = false;
    }

    void invalidate()
    {
    }

    void set_frost(float f)
    {
        const float v = clampf(f, 0.f, 1.f);
        g_frost.store(v);
        if (g_compositor && g_dcomp_ok)
        {
            const float fa = std::min(0.85f, 0.12f + 0.55f * v);
            g_compositor->SetTintColor(D2D1::ColorF(0.055f, 0.055f, 0.063f, fa), true);
        }
        else if (g_accent_fb && g_acrylic)
        {
            // accent fallback: re-apply with a new tint alpha
            const BYTE a = (BYTE)(0x40 + 0x90 * v);
            const auto lib = LoadLibraryW(L"user32.dll");
            if (lib)
            {
                typedef BOOL(WINAPI* pFn)(HWND, WINCOMPATTRDATA*);
                const auto fn = (pFn)GetProcAddress(lib, "SetWindowCompositionAttribute");
                if (fn)
                {
                    ACCENTPOLICY policy{ 4, 2, (int)(0x63000000u | ((DWORD)a << 16) | 0x0D05u), 0 };
                    WINCOMPATTRDATA data{ 19, &policy, sizeof(ACCENTPOLICY) };
                    fn(g_acrylic, &data);
                }
                FreeLibrary(lib);
            }
        }
    }

    void set_blur(float f)
    {
        const float v = clampf(f, 0.f, 100.f);
        g_blur.store(v);
        if (g_compositor && g_dcomp_ok)
            g_compositor->SetBlurAmount(v);
    }

    void set_menu_rect(float x, float y, float w, float h)
    {
        if (w <= 1.f || h <= 1.f)
        {
            g_rects.clear();
            g_rounds.clear();
            commit();
            return;
        }
        new_frame();
        add_rect(x, y, w, h, 8.f);
        commit();
    }

    // --- multi-window acrylic ------------------------------------------------

    void new_frame()
    {
        g_rects.clear();
        g_rounds.clear();
    }

    void add_rect(float x, float y, float w, float h, float rounding)
    {
        if (w <= 1.f || h <= 1.f)
            return;
        g_rects.push_back(RECT{ (int)x, (int)y, (int)(x + w), (int)(y + h) });
        g_rounds.push_back(clampf(rounding, 0.f, 32.f));
    }

    void commit()
    {
        if (g_rects.empty())
        {
            if (g_acrylic && IsWindowVisible(g_acrylic))
                ShowWindow(g_acrylic, SW_HIDE);
            g_haveLast = false;
            return;
        }

        if (!CreateAcrylicWindow())
            return;

        // bounding box of all glass-backed windows
        RECT bbox = g_rects[0];
        for (size_t i = 1; i < g_rects.size(); ++i)
        {
            bbox.left   = std::min(bbox.left,   g_rects[i].left);
            bbox.top    = std::min(bbox.top,    g_rects[i].top);
            bbox.right  = std::max(bbox.right,  g_rects[i].right);
            bbox.bottom = std::max(bbox.bottom, g_rects[i].bottom);
        }

        const bool wasHidden = !IsWindowVisible(g_acrylic);
        const bool moved = !g_haveLast || memcmp(&bbox, &g_lastRect, sizeof(RECT)) != 0;
        // compare rects RELATIVE to the bbox: while dragging the whole window group
        // the relative layout is unchanged, so we skip the expensive region rebuild
        std::vector<RECT> norm;
        norm.reserve(g_rects.size());
        for (size_t i = 0; i < g_rects.size(); ++i)
            norm.push_back(RECT{ g_rects[i].left - bbox.left, g_rects[i].top - bbox.top, g_rects[i].right - bbox.left, g_rects[i].bottom - bbox.top });
        const bool layoutChanged = norm != g_lastNorm;


        // ImGui coords are relative to the overlay window, which follows the
        // game client rect. Convert to screen coords so the acrylic popup
        // stays glued to the UI even when the game is not at (0,0).
        POINT origin = { 0, 0 };
        if (HWND overlay = Cheat::Renderer::GetHwnd())
            ClientToScreen(overlay, &origin);
        const RECT sbbox = { bbox.left + origin.x, bbox.top + origin.y,
                             bbox.right + origin.x, bbox.bottom + origin.y };
        // SCREEN-space compare: repositions on menu drags AND game-window moves
        const bool movedScreen = !g_haveScreen || memcmp(&sbbox, &g_lastScreenRect, sizeof(RECT)) != 0;

        if (movedScreen)
        {
            SetWindowPos(g_acrylic, nullptr, sbbox.left, sbbox.top,
                sbbox.right - sbbox.left, sbbox.bottom - sbbox.top,
                SWP_NOACTIVATE | SWP_NOZORDER); // fires WM_WINDOWPOSCHANGED -> SyncCoordinates
            g_lastScreenRect = sbbox;
            g_haveScreen = true;
        }

        if (layoutChanged)
        {
            // union of rounded regions, one per window, relative to the bbox
            HRGN acc = CreateRectRgn(0, 0, 0, 0);
            for (size_t i = 0; i < g_rects.size(); ++i)
            {
                const RECT& r = g_rects[i];
                const int d = (int)(g_rounds[i] * 2.f);
                HRGN part = CreateRoundRectRgn(
                    r.left - bbox.left, r.top - bbox.top,
                    r.right - bbox.left + 1, r.bottom - bbox.top + 1,
                    d > 0 ? d : 1, d > 0 ? d : 1);
                CombineRgn(acc, acc, part, RGN_OR);
                DeleteObject(part);
            }
            SetWindowRgn(g_acrylic, acc, FALSE); // system owns acc now
            g_lastNorm = norm;
        }

        if (wasHidden)
        {
            ShowWindow(g_acrylic, SW_SHOWNOACTIVATE);
        }

        if (!g_compositor && !g_accent_fb)
        {
            AttemptInit();          // window is visible first, compose second (reference order)
            if (g_compositor && g_dcomp_ok)
                g_compositor->Flush();
        }
        else if ((wasHidden || movedScreen) && g_compositor && g_dcomp_ok)
        {
            g_compositor->SyncCoordinates2(g_acrylic);
            // reference behavior on move: only re-sync clip/transform (Commit+DwmFlush).
            // re-calling the ordinal-164 update mid-drag makes DWM re-resolve the
            // "windows behind" set while z-order is in flux -> wallpaper flashes.
            if (wasHidden)
                g_compositor->Flush();  // full thumbnail refresh after re-show
        }
  }
    void draw(ImDrawList* draw_list, const ImVec2& rect_min, const ImVec2& rect_max, float rounding, float alpha)
    {
        if (!draw_list) return;
        // light wash + top sheen over the DWM acrylic
        const ImU32 wash = IM_COL32(14, 14, 18, (ImU32)(86 * alpha));
        draw_list->AddRectFilled(rect_min, rect_max, wash, rounding);
        const float sheenH = (rect_max.y - rect_min.y) * 0.35f;
        draw_list->AddRectFilledMultiColor(rect_min, ImVec2(rect_max.x, rect_min.y + sheenH),
            IM_COL32(255, 255, 255, (ImU32)(14 * alpha)), IM_COL32(255, 255, 255, (ImU32)(14 * alpha)),
            IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));
        draw_list->AddRect(rect_min, rect_max, IM_COL32(235, 240, 237, (ImU32)(70 * alpha)), rounding, 0, 1.2f);
    }
}
