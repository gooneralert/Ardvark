#include "backdrop_blur.h"

#include <d3dcompiler.h>
#include <cstring>
#include <windows.h>

#include <game/game.h>

#pragma comment(lib, "d3dcompiler.lib")

namespace
{
	constexpr int k_max_w = 640;
	constexpr int k_max_h = 360;

	constexpr char k_hlsl[] = R"HLSL(
cbuffer Constants : register(b0)
{
    float  blur_scale;
    float2 texel;
    float  _pad0;
    float2 direction;
    float2 _pad1;
};

struct PSIn
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

PSIn vs_main(uint id : SV_VertexID)
{
    PSIn o;
    o.uv  = float2((id << 1) & 2, id & 2);
    o.pos = float4(o.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return o;
}

Texture2D    src_tex : register(t0);
SamplerState src_smp : register(s0);

float4 ps_copy(PSIn i) : SV_TARGET
{
    return src_tex.Sample(src_smp, i.uv);
}

float4 ps_blur(PSIn i) : SV_TARGET
{
    const float offsets[3] = { 0.0, 1.3846153846, 3.2307692308 };
    const float weights[3] = { 0.2270270270, 0.3162162162, 0.0702702703 };

    float4 c = src_tex.Sample(src_smp, i.uv) * weights[0];
    [unroll]
    for (int k = 1; k < 3; ++k)
    {
        float2 off = direction * offsets[k] * texel * blur_scale;
        c += src_tex.Sample(src_smp, i.uv + off) * weights[k];
        c += src_tex.Sample(src_smp, i.uv - off) * weights[k];
    }
    return c;
}
)HLSL";

	struct alignas(16) Constants
	{
		float blur_scale;
		float texel_x;
		float texel_y;
		float pad0;
		float dir_x;
		float dir_y;
		float pad1;
		float pad2;
	};

	ID3D11Device*        g_device  = nullptr;
	ID3D11DeviceContext* g_context = nullptr;

	ID3D11VertexShader*      g_vs = nullptr;
	ID3D11PixelShader*       g_ps_copy = nullptr;
	ID3D11PixelShader*       g_ps_blur = nullptr;
	ID3D11Buffer*            g_cb = nullptr;
	ID3D11SamplerState*      g_samp = nullptr;
	ID3D11RasterizerState*   g_rs = nullptr;
	ID3D11BlendState*        g_bs = nullptr;
	ID3D11DepthStencilState* g_dss = nullptr;

	ID3D11Texture2D*          g_src_tex = nullptr;
	ID3D11ShaderResourceView* g_src_srv = nullptr;
	ID3D11Texture2D*          g_tex[2] = {};
	ID3D11RenderTargetView*   g_rtv[2] = {};
	ID3D11ShaderResourceView* g_srv[2] = {};

	HDC     g_screen_dc = nullptr;
	HDC     g_mem_dc    = nullptr;
	HBITMAP g_bitmap    = nullptr;
	void*   g_bits      = nullptr;
	int     g_cap_w = 0;
	int     g_cap_h = 0;

	float g_origin_x = 0.f;
	float g_origin_y = 0.f;
	float g_src_w = 1.f;
	float g_src_h = 1.f;

	bool  g_ready = false;
	bool  g_paused = false;
	float g_last_time = -1.f;

	template <typename T>
	void release(T*& p)
	{
		if (p) { p->Release(); p = nullptr; }
	}

	void release_gdi()
	{
		if (g_bitmap)    { DeleteObject(g_bitmap); g_bitmap = nullptr; }
		if (g_mem_dc)    { DeleteDC(g_mem_dc); g_mem_dc = nullptr; }
		if (g_screen_dc) { ReleaseDC(nullptr, g_screen_dc); g_screen_dc = nullptr; }
		g_bits = nullptr;
	}

	void release_rts()
	{
		release(g_src_srv);
		release(g_src_tex);
		for (int i = 0; i < 2; ++i)
		{
			release(g_srv[i]);
			release(g_rtv[i]);
			release(g_tex[i]);
		}
		g_cap_w = g_cap_h = 0;
	}

	bool create_rt(unsigned idx, unsigned w, unsigned h)
	{
		D3D11_TEXTURE2D_DESC td{};
		td.Width = w;
		td.Height = h;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		if (FAILED(g_device->CreateTexture2D(&td, nullptr, &g_tex[idx])))
			return false;
		if (FAILED(g_device->CreateRenderTargetView(g_tex[idx], nullptr, &g_rtv[idx])))
			return false;
		if (FAILED(g_device->CreateShaderResourceView(g_tex[idx], nullptr, &g_srv[idx])))
			return false;
		return true;
	}

	bool ensure_targets(int w, int h)
	{
		if (w < 8) w = 8;
		if (h < 8) h = 8;
		if (w == g_cap_w && h == g_cap_h && g_src_tex && g_tex[0] && g_tex[1] && g_bits)
			return true;

		release_rts();
		release_gdi();

		D3D11_TEXTURE2D_DESC td{};
		td.Width = (UINT)w;
		td.Height = (UINT)h;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		if (FAILED(g_device->CreateTexture2D(&td, nullptr, &g_src_tex)))
			return false;
		if (FAILED(g_device->CreateShaderResourceView(g_src_tex, nullptr, &g_src_srv)))
			return false;
		if (!create_rt(0, (unsigned)w, (unsigned)h) || !create_rt(1, (unsigned)w, (unsigned)h))
			return false;

		g_screen_dc = GetDC(nullptr);
		if (!g_screen_dc)
			return false;
		g_mem_dc = CreateCompatibleDC(g_screen_dc);
		if (!g_mem_dc)
			return false;

		BITMAPINFO bmi{};
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = w;
		bmi.bmiHeader.biHeight = -h;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;
		g_bitmap = CreateDIBSection(g_mem_dc, &bmi, DIB_RGB_COLORS, &g_bits, nullptr, 0);
		if (!g_bitmap || !g_bits)
			return false;
		SelectObject(g_mem_dc, g_bitmap);
		g_cap_w = w;
		g_cap_h = h;
		return true;
	}

	void draw_quad(ID3D11PixelShader* ps, ID3D11RenderTargetView* rtv, ID3D11ShaderResourceView* src, const Constants& cb)
	{
		D3D11_VIEWPORT vp{};
		vp.Width = (float)g_cap_w;
		vp.Height = (float)g_cap_h;
		vp.MaxDepth = 1.f;

		g_context->OMSetRenderTargets(1, &rtv, nullptr);
		g_context->RSSetViewports(1, &vp);
		g_context->IASetInputLayout(nullptr);
		g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		g_context->VSSetShader(g_vs, nullptr, 0);
		g_context->PSSetShader(ps, nullptr, 0);
		g_context->RSSetState(g_rs);
		g_context->OMSetBlendState(g_bs, nullptr, 0xffffffff);
		g_context->OMSetDepthStencilState(g_dss, 0);
		g_context->PSSetSamplers(0, 1, &g_samp);

		D3D11_MAPPED_SUBRESOURCE mapped{};
		if (SUCCEEDED(g_context->Map(g_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			std::memcpy(mapped.pData, &cb, sizeof(cb));
			g_context->Unmap(g_cb, 0);
		}
		g_context->VSSetConstantBuffers(0, 1, &g_cb);
		g_context->PSSetConstantBuffers(0, 1, &g_cb);

		ID3D11ShaderResourceView* srvs[1] = { src };
		g_context->PSSetShaderResources(0, 1, srvs);
		g_context->Draw(3, 0);

		ID3D11ShaderResourceView* none[1] = { nullptr };
		g_context->PSSetShaderResources(0, 1, none);
		ID3D11RenderTargetView* null_rtv = nullptr;
		g_context->OMSetRenderTargets(1, &null_rtv, nullptr);
	}

	bool capture()
	{
		int sx = 0, sy = 0, sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
		HWND game_wnd = game::get_roblox_window();
		if (game_wnd && IsWindow(game_wnd))
		{
			RECT crc{};
			if (GetClientRect(game_wnd, &crc))
			{
				POINT origin{ 0, 0 };
				ClientToScreen(game_wnd, &origin);
				sx = origin.x;
				sy = origin.y;
				sw = crc.right - crc.left;
				sh = crc.bottom - crc.top;
			}
		}
		if (sw < 8 || sh < 8)
			return false;

		g_origin_x = (float)sx;
		g_origin_y = (float)sy;
		g_src_w = (float)sw;
		g_src_h = (float)sh;

		int dw = sw;
		int dh = sh;
		if (dw > k_max_w || dh > k_max_h)
		{
			const float s = (dw > dh)
				? (float)k_max_w / (float)dw
				: (float)k_max_h / (float)dh;
			dw = (int)(dw * s);
			dh = (int)(dh * s);
		}
		if (dw < 8) dw = 8;
		if (dh < 8) dh = 8;
		if (!ensure_targets(dw, dh))
			return false;

		SetStretchBltMode(g_mem_dc, HALFTONE);
		SetBrushOrgEx(g_mem_dc, 0, 0, nullptr);

		// Capture the game window only. Desktop BitBlt includes this overlay
		// and used to hide it via WDA_EXCLUDEFROMCAPTURE, which strobes
		// OBS/Medal. PrintWindow never needs that toggle.
		BOOL ok = FALSE;
#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif
		if (game_wnd && IsWindow(game_wnd))
			ok = PrintWindow(game_wnd, g_mem_dc, PW_RENDERFULLCONTENT);
		if (!ok)
			return false;

		if (!ok || !g_bits)
			return false;

		g_context->UpdateSubresource(g_src_tex, 0, nullptr, g_bits, (UINT)(dw * 4), 0);
		return true;
	}
}

namespace backdrop_blur
{
	bool init(ID3D11Device* device, ID3D11DeviceContext* context)
	{
		shutdown();
		if (!device || !context)
			return false;

		g_device = device;
		g_context = context;

		ID3DBlob* vs_blob = nullptr;
		ID3DBlob* ps_copy_blob = nullptr;
		ID3DBlob* ps_blur_blob = nullptr;
		ID3DBlob* err = nullptr;

		auto compile = [&](const char* entry, const char* profile, ID3DBlob** out) -> bool
		{
			err = nullptr;
			HRESULT hr = D3DCompile(k_hlsl, sizeof(k_hlsl) - 1, "backdrop_blur", nullptr, nullptr,
				entry, profile, 0, 0, out, &err);
			if (FAILED(hr))
			{
				if (err) err->Release();
				return false;
			}
			if (err) err->Release();
			return true;
		};

		if (!compile("vs_main", "vs_4_0", &vs_blob) ||
			!compile("ps_copy", "ps_4_0", &ps_copy_blob) ||
			!compile("ps_blur", "ps_4_0", &ps_blur_blob))
		{
			if (vs_blob) vs_blob->Release();
			if (ps_copy_blob) ps_copy_blob->Release();
			if (ps_blur_blob) ps_blur_blob->Release();
			return false;
		}

		g_device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, &g_vs);
		g_device->CreatePixelShader(ps_copy_blob->GetBufferPointer(), ps_copy_blob->GetBufferSize(), nullptr, &g_ps_copy);
		g_device->CreatePixelShader(ps_blur_blob->GetBufferPointer(), ps_blur_blob->GetBufferSize(), nullptr, &g_ps_blur);
		vs_blob->Release();
		ps_copy_blob->Release();
		ps_blur_blob->Release();

		D3D11_BUFFER_DESC cbd{};
		cbd.ByteWidth = sizeof(Constants);
		cbd.Usage = D3D11_USAGE_DYNAMIC;
		cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		if (FAILED(g_device->CreateBuffer(&cbd, nullptr, &g_cb)))
			return false;

		D3D11_SAMPLER_DESC sd{};
		sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		sd.MaxLOD = D3D11_FLOAT32_MAX;
		g_device->CreateSamplerState(&sd, &g_samp);

		D3D11_RASTERIZER_DESC rd{};
		rd.FillMode = D3D11_FILL_SOLID;
		rd.CullMode = D3D11_CULL_NONE;
		g_device->CreateRasterizerState(&rd, &g_rs);

		D3D11_BLEND_DESC bd{};
		bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		g_device->CreateBlendState(&bd, &g_bs);

		D3D11_DEPTH_STENCIL_DESC dsd{};
		dsd.DepthEnable = FALSE;
		dsd.StencilEnable = FALSE;
		g_device->CreateDepthStencilState(&dsd, &g_dss);

		g_ready = true;
		return true;
	}

	void shutdown()
	{
		g_ready = false;
		g_paused = false;
		g_last_time = -1.f;
		release_rts();
		release_gdi();
		release(g_dss);
		release(g_bs);
		release(g_rs);
		release(g_samp);
		release(g_cb);
		release(g_ps_blur);
		release(g_ps_copy);
		release(g_vs);
		g_device = nullptr;
		g_context = nullptr;
	}

	void set_paused(bool paused)
	{
		if (g_paused && !paused)
			g_last_time = -1.f;
		g_paused = paused;
	}

	void update()
	{
		// fill() draws a solid panel. PrintWindow(PW_RENDERFULLCONTENT) was
		// capturing the game every few hundred ms for a texture that is never
		// sampled — and that capture stalls both the overlay and Roblox.
	}

	void fill(ImDrawList* dl, ImVec2 min, ImVec2 max, float rounding, ImU32 tint)
	{
		if (!dl)
			return;
		const int r = (tint >> IM_COL32_R_SHIFT) & 0xFF;
		const int g = (tint >> IM_COL32_G_SHIFT) & 0xFF;
		const int b = (tint >> IM_COL32_B_SHIFT) & 0xFF;
		int a = (tint >> IM_COL32_A_SHIFT) & 0xFF;
		if (a >= 230)
			a = 255;
		dl->AddRectFilled(min, max, IM_COL32(r, g, b, a), rounding);
	}
}
