#pragma once
// только из Renderer.cpp

#include "Renderer.h"
#include <d3d11.h>
#include "app/Graphics.h"
#include "features/visuals/mesh/MeshDxShader.h"

namespace Cheat {

	void Renderer::ResizeSwapchain(int w, int h)
	{
		if (!m_SwapChain || w <= 0 || h <= 0)
		{
			return;
		}

		if (w == m_Width && h == m_Height)
		{
			return;
		}

		CleanupRenderTarget();
		if (FAILED(m_SwapChain->ResizeBuffers(0, (UINT)w, (UINT)h, DXGI_FORMAT_UNKNOWN, 0)))
		{
			return;
		}

		CreateRenderTarget();
		m_Width = w;
		m_Height = h;
		Visuals::MeshDxShader::Resize((unsigned)w, (unsigned)h);
	}

	bool Renderer::CreateDevice()
	{
		DXGI_SWAP_CHAIN_DESC sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.BufferCount = 2;
		sd.BufferDesc.Width = 0;
		sd.BufferDesc.Height = 0;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 0;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = m_Hwnd;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = TRUE;
		sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		UINT flags = 0;
		D3D_FEATURE_LEVEL level;
		D3D_FEATURE_LEVEL levels[2] = {
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_0,
		};

		HRESULT hr = D3D11CreateDeviceAndSwapChain(
			nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
			levels, 2, D3D11_SDK_VERSION, &sd,
			&m_SwapChain, &m_Device, &level, &m_DeviceContext);
		if (hr != S_OK)
		{
			return false;
		}

		CreateRenderTarget();

		DXGI_SWAP_CHAIN_DESC cur{};
		if (SUCCEEDED(m_SwapChain->GetDesc(&cur)))
		{
			m_Width = (int)cur.BufferDesc.Width;
			m_Height = (int)cur.BufferDesc.Height;
		}
		return true;
	}

	void Renderer::CreateRenderTarget()
	{
		ID3D11Texture2D* back = nullptr;
		m_SwapChain->GetBuffer(0, IID_PPV_ARGS(&back));
		if (!back)
		{
			return;
		}

		m_Device->CreateRenderTargetView(back, nullptr, &m_RenderTargetView);
		back->Release();

		Cheat::Core::g_RenderTargetView = m_RenderTargetView;
		Cheat::Core::g_SwapChain = m_SwapChain;
	}

	void Renderer::CleanupDevice()
	{
		CleanupRenderTarget();

		if (m_SwapChain)
		{
			m_SwapChain->Release();
			m_SwapChain = nullptr;
		}

		if (m_DeviceContext)
		{
			m_DeviceContext->Release();
			m_DeviceContext = nullptr;
		}

		if (m_Device)
		{
			m_Device->Release();
			m_Device = nullptr;
		}
	}

	void Renderer::CleanupRenderTarget()
	{
		if (m_RenderTargetView)
		{
			m_RenderTargetView->Release();
			m_RenderTargetView = nullptr;
		}
		Cheat::Core::g_RenderTargetView = nullptr;
	}

}
