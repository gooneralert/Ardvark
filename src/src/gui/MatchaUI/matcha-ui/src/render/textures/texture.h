#pragma once
#include <d3d11.h>

ID3D11ShaderResourceView* D3D11CreateTextureFromBytes(
    ID3D11Device* pDevice,
    LPCVOID pSrcData,
    SIZE_T SrcDataSize
);

// Creates a texture from raw PNG bytes with full mipchain (uses stb_image).
// Requires the device context for GenerateMips.
ID3D11ShaderResourceView* D3D11CreateTextureWithMips(
    ID3D11Device*        pDevice,
    ID3D11DeviceContext* pContext,
    const void*          pPngData,
    size_t               PngDataSize
);

// Like D3D11CreateTextureWithMips but CPU box-filter downscales to (target_w x target_h)
// before upload — eliminates aliasing when displaying small icons.
ID3D11ShaderResourceView* D3D11CreateTextureScaled(
    ID3D11Device*        pDevice,
    ID3D11DeviceContext* pContext,
    const void*          pPngData,
    size_t               PngDataSize,
    int                  target_w,
    int                  target_h,
    bool                 as_alpha_mask = false
);