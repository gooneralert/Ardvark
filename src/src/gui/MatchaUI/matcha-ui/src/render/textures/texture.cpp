#include "texture.h"

#include "../../../ext/imgui/include/D3DX11tex.h"
#include <stdlib.h>
#include <cstring>

// stb_image is already compiled in another TU - just declare what we need
extern "C" unsigned char* stbi_load_from_memory(const unsigned char*, int, int*, int*, int*, int);
extern "C" void           stbi_image_free(void*);

ID3D11ShaderResourceView* D3D11CreateTextureFromBytes(
    ID3D11Device* pDevice,
    LPCVOID pSrcData,
    SIZE_T SrcDataSize
)
{
    D3DX11_IMAGE_LOAD_INFO temp_info = {};
    ID3DX11ThreadPump* temp_pump = nullptr;
    ID3D11ShaderResourceView* pTexture = nullptr;

    HRESULT hResult;

    hResult = D3DX11CreateShaderResourceViewFromMemory(pDevice, pSrcData, SrcDataSize, &temp_info, temp_pump, &pTexture, 0);

    if (FAILED(hResult))
        return nullptr;

    return pTexture;
}

ID3D11ShaderResourceView* D3D11CreateTextureWithMips(
    ID3D11Device*        pDevice,
    ID3D11DeviceContext* pContext,
    const void*          pPngData,
    size_t               PngDataSize
)
{
    int w, h, ch;
    unsigned char* pixels = stbi_load_from_memory(
        (const unsigned char*)pPngData, (int)PngDataSize, &w, &h, &ch, 4);
    if (!pixels) return nullptr;

    // Create texture with MipLevels=0 (full chain) and GENERATE_MIPS bind flag
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width              = (UINT)w;
    desc.Height             = (UINT)h;
    desc.MipLevels          = 0; // full mip chain
    desc.ArraySize          = 1;
    desc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count   = 1;
    desc.Usage              = D3D11_USAGE_DEFAULT;
    desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags          = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    ID3D11Texture2D* tex = nullptr;
    if (FAILED(pDevice->CreateTexture2D(&desc, nullptr, &tex))) {
        stbi_image_free(pixels);
        return nullptr;
    }

    // Upload mip 0
    pContext->UpdateSubresource(tex, 0, nullptr, pixels, (UINT)(w * 4), 0);
    stbi_image_free(pixels);

    // Create SRV then generate mips
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format                    = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels       = (UINT)-1; // all levels
    srvDesc.Texture2D.MostDetailedMip = 0;

    ID3D11ShaderResourceView* srv = nullptr;
    if (FAILED(pDevice->CreateShaderResourceView(tex, &srvDesc, &srv))) {
        tex->Release();
        return nullptr;
    }
    tex->Release();

    pContext->GenerateMips(srv);
    return srv;
}

ID3D11ShaderResourceView* D3D11CreateTextureScaled(
    ID3D11Device*        pDevice,
    ID3D11DeviceContext* pContext,
    const void*          pPngData,
    size_t               PngDataSize,
    int                  target_w,
    int                  target_h,
    bool                 as_alpha_mask
)
{
    int src_w, src_h, ch;
    unsigned char* src = stbi_load_from_memory(
        (const unsigned char*)pPngData, (int)PngDataSize,
        &src_w, &src_h, &ch, 4);
    if (!src) return nullptr;

    // CPU box-filter downscale: each output texel averages over its source footprint
    unsigned char* dst = (unsigned char*)malloc((size_t)target_w * target_h * 4);
    if (!dst) { stbi_image_free(src); return nullptr; }

    for (int dy = 0; dy < target_h; ++dy)
    {
        const int sy0  = (dy       * src_h) / target_h;
        const int sy1  = ((dy + 1) * src_h) / target_h;
        const int rows = sy1 - sy0 > 0 ? sy1 - sy0 : 1;

        for (int dx = 0; dx < target_w; ++dx)
        {
            const int sx0  = (dx       * src_w) / target_w;
            const int sx1  = ((dx + 1) * src_w) / target_w;
            const int cols = sx1 - sx0 > 0 ? sx1 - sx0 : 1;
            const int n    = rows * cols;

            unsigned int r = 0, g = 0, b = 0, a = 0;
            for (int sy = sy0; sy < sy0 + rows; ++sy)
            {
                const unsigned char* row = src + sy * src_w * 4;
                for (int sx = sx0; sx < sx0 + cols; ++sx)
                {
                    const unsigned char* px = row + sx * 4;
                    r += px[0]; g += px[1]; b += px[2]; a += px[3];
                }
            }
            unsigned char* out = dst + (dy * target_w + dx) * 4;
            if (as_alpha_mask)
            {
                out[0] = out[1] = out[2] = 255;
                out[3] = (unsigned char)(a / n);
            }
            else
            {
                out[0] = (unsigned char)(r / n);
                out[1] = (unsigned char)(g / n);
                out[2] = (unsigned char)(b / n);
                out[3] = (unsigned char)(a / n);
            }
        }
    }
    stbi_image_free(src);

    // Upload the small pre-filtered image with a full mip chain
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width            = (UINT)target_w;
    desc.Height           = (UINT)target_h;
    desc.MipLevels        = 0;
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_DEFAULT;
    desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags        = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    ID3D11Texture2D* tex = nullptr;
    if (FAILED(pDevice->CreateTexture2D(&desc, nullptr, &tex))) {
        free(dst);
        return nullptr;
    }
    pContext->UpdateSubresource(tex, 0, nullptr, dst, (UINT)(target_w * 4), 0);
    free(dst);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format                    = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels       = (UINT)-1;
    srvDesc.Texture2D.MostDetailedMip = 0;

    ID3D11ShaderResourceView* srv = nullptr;
    if (FAILED(pDevice->CreateShaderResourceView(tex, &srvDesc, &srv))) {
        tex->Release();
        return nullptr;
    }
    tex->Release();
    pContext->GenerateMips(srv);
    return srv;
}