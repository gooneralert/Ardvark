#pragma once
#include <d3d11.h>

/* dx11 globals */

namespace Cheat {
namespace Core {

    inline ID3D11Device* g_Device = nullptr;
    inline ID3D11DeviceContext* g_DeviceContext = nullptr;
    inline ID3D11RenderTargetView* g_RenderTargetView = nullptr;
    inline IDXGISwapChain* g_SwapChain = nullptr;

}
}
