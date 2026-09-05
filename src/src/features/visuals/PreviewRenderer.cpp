#include "pch.h"
#define NOMINMAX
#include "PreviewRenderer.h"
#include "app/Graphics.h"
#include "gui/colors/colors.h"
#include <stb_image.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <map>
#include <sstream>
#include <unordered_map>

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

static const char s_VS[] = R"(
cbuffer CB : register(b0) {
    float4x4 g_MVP;
    float4x4 g_World;
    float4   g_Opacity;
    float4   g_Diffuse;
};
struct VS_In {
    float3 pos : POSITION;
    float3 nrm : NORMAL;
    float2 uv  : TEXCOORD;
};
struct VS_Out {
    float4 pos : SV_POSITION;
    float3 nrm : NORMAL;
    float2 uv  : TEXCOORD;
};
VS_Out main(VS_In v) {
    VS_Out o;
    o.pos = mul(float4(v.pos, 1.0), g_MVP);
    o.nrm = mul(v.nrm, (float3x3)g_World);
    o.uv  = v.uv;
    return o;
}
)";
static const char s_PS[] = R"(
cbuffer CB : register(b0) {
    float4x4 g_MVP;
    float4x4 g_World;
    float4   g_Opacity;
    float4   g_Diffuse;
};
Texture2D    g_Tex  : register(t0);
SamplerState g_Sam  : register(s0);
struct VS_Out {
    float4 pos : SV_POSITION;
    float3 nrm : NORMAL;
    float2 uv  : TEXCOORD;
};
float4 main(VS_Out v) : SV_Target {
    float4 tex = g_Tex.Sample(g_Sam, v.uv);
    float3 n = normalize(v.nrm);
    float3 L = normalize(float3(0.45, 0.85, -0.35));
    float3 H = normalize(L + float3(0.0, 0.15, -1.0));
    float ndotl = saturate(dot(n, L));
    float spec = pow(saturate(dot(n, H)), 32.0) * 0.18;
    float wrap = saturate(ndotl * 0.65 + 0.35);
    float3 lit = tex.rgb * g_Diffuse.rgb * (0.28 + wrap * 0.82) + spec;
    lit = saturate((lit - 0.5) * 1.08 + 0.5);
    return float4(lit, saturate(g_Opacity.x));
}
)";

namespace Cheat::Core {

static bool CompileShader(const char* src, const char* entry, const char* profile, ID3DBlob** out)
{
    ID3DBlob* err = nullptr;
    HRESULT hr = D3DCompile(src, strlen(src), nullptr, nullptr, nullptr,
                            entry, profile, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, out, &err);
    if (err) err->Release(); // â•¨â•›â•¤Ãªâ•¨â••â•¨â–’â•¨â•‘â•¨â•• â•¨â•œâ•¨â–‘â•¨â• â•¨â”â•¨â•›â•¤Ã¤â•¨â••â•¨â”‚, SUCCEEDED â•¤Ã â•¨â–“â•¨â–‘â•¤Ã©â•¨â••â•¤Ã©
    return SUCCEEDED(hr);
}

bool PreviewRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* ctx,
                                 unsigned int rt_width, unsigned int rt_height)
{
    m_Device = device;
    m_Ctx = ctx;
    m_Width = rt_width;
    m_Height = rt_height;

    if (!CreateRenderTarget(rt_width, rt_height)) return false;
    if (!CreateShaders()) return false;

    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    rd.MultisampleEnable = TRUE;
    m_Device->CreateRasterizerState(&rd, &m_RS);

    D3D11_DEPTH_STENCIL_DESC dd{};
    dd.DepthEnable = TRUE;
    dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dd.DepthFunc = D3D11_COMPARISON_LESS;
    m_Device->CreateDepthStencilState(&dd, &m_DSState);

    D3D11_DEPTH_STENCIL_DESC dd2 = dd;
    dd2.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    m_Device->CreateDepthStencilState(&dd2, &m_DSNoWrite);

    D3D11_BLEND_DESC bld{};
    bld.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    m_Device->CreateBlendState(&bld, &m_BlendOpaque);

    D3D11_BLEND_DESC abl{};
    abl.RenderTarget[0].BlendEnable = TRUE;
    abl.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    abl.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    abl.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    abl.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    abl.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    abl.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    abl.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    m_Device->CreateBlendState(&abl, &m_BlendAlpha);

    m_Ready = true;
    return true;
}

bool PreviewRenderer::CreateRenderTarget(unsigned int w, unsigned int h)
{
	m_SampleCount = 1;
	UINT quality = 0;
	if (SUCCEEDED(m_Device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 4, &quality)) && quality > 0)
		m_SampleCount = 4;

	else if (SUCCEEDED(m_Device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 2, &quality)) && quality > 0)
		m_SampleCount = 2;

	D3D11_TEXTURE2D_DESC td{};
	td.Width = w;
	td.Height = h;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	td.SampleDesc.Count = m_SampleCount;
	td.SampleDesc.Quality = 0;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_RENDER_TARGET;
	if (FAILED(m_Device->CreateTexture2D(&td, nullptr, &m_RTTex)))
		return false;
	if (FAILED(m_Device->CreateRenderTargetView(m_RTTex, nullptr, &m_RTV)))
		return false;

	D3D11_TEXTURE2D_DESC rd = td;
	rd.SampleDesc.Count = 1;
	rd.SampleDesc.Quality = 0;
	rd.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	if (FAILED(m_Device->CreateTexture2D(&rd, nullptr, &m_ResolveTex)))
		return false;
	if (FAILED(m_Device->CreateShaderResourceView(m_ResolveTex, nullptr, &m_SRV)))
		return false;

	D3D11_TEXTURE2D_DESC dd{};
	dd.Width = w;
	dd.Height = h;
	dd.MipLevels = 1;
	dd.ArraySize = 1;
	dd.Format = DXGI_FORMAT_D32_FLOAT;
	dd.SampleDesc.Count = m_SampleCount;
	dd.SampleDesc.Quality = 0;
	dd.Usage = D3D11_USAGE_DEFAULT;
	dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	if (FAILED(m_Device->CreateTexture2D(&dd, nullptr, &m_DepthTex)))
		return false;
	return SUCCEEDED(m_Device->CreateDepthStencilView(m_DepthTex, nullptr, &m_DSV));
}

bool PreviewRenderer::CreateShaders()
{
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    if (!CompileShader(s_VS, "main", "vs_4_0", &vsBlob)) return false;
    if (!CompileShader(s_PS, "main", "ps_4_0", &psBlob)) { vsBlob->Release(); return false; }

    HRESULT hr = m_Device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_VS);
    if (SUCCEEDED(hr))
        hr = m_Device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_PS);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    if (SUCCEEDED(hr))
        hr = m_Device->CreateInputLayout(layout, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_IL);

    vsBlob->Release();
    psBlob->Release();
    if (FAILED(hr)) return false;

    D3D11_BUFFER_DESC cbd{};
    cbd.ByteWidth = 160;
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    return SUCCEEDED(m_Device->CreateBuffer(&cbd, nullptr, &m_CB));
}

namespace {
    bool UploadVerts(ID3D11Device* device, std::vector<ModelVertex>& verts,
                     ID3D11Buffer** ppVB, unsigned int& vertCount)
    {
        if (verts.empty()) return false;
        if (*ppVB) { (*ppVB)->Release(); *ppVB = nullptr; }
        vertCount = (unsigned int)verts.size();
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = (UINT)(sizeof(ModelVertex) * verts.size());
        bd.Usage = D3D11_USAGE_IMMUTABLE;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA sd{};
        sd.pSysMem = verts.data();
        return SUCCEEDED(device->CreateBuffer(&bd, &sd, ppVB));
    }

    bool UploadPng(ID3D11Device* device, const stbi_uc* data, int w, int h,
                   ID3D11Texture2D** ppTex, ID3D11ShaderResourceView** ppSRV,
                   ID3D11SamplerState** ppSamp)
    {
        if (*ppSRV) { (*ppSRV)->Release(); *ppSRV = nullptr; }
        if (*ppTex) { (*ppTex)->Release(); *ppTex = nullptr; }
        if (*ppSamp) { (*ppSamp)->Release(); *ppSamp = nullptr; }

        D3D11_TEXTURE2D_DESC td{};
        td.Width = (UINT)w; td.Height = (UINT)h; td.MipLevels = 0; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        td.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
        if (FAILED(device->CreateTexture2D(&td, nullptr, ppTex))) return false;

        D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
        srvd.Format = td.Format;
        srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvd.Texture2D.MipLevels = (UINT)-1;
        if (FAILED(device->CreateShaderResourceView(*ppTex, &srvd, ppSRV))) return false;

        ID3D11DeviceContext* ctx = nullptr;
        device->GetImmediateContext(&ctx);
        if (ctx) {
            ctx->UpdateSubresource(*ppTex, 0, nullptr, data, (UINT)(w * 4), 0);
            ctx->GenerateMips(*ppSRV);
            ctx->Release();
        }

        D3D11_SAMPLER_DESC smd{};
        smd.Filter = D3D11_FILTER_ANISOTROPIC;
        smd.MaxAnisotropy = 8;
        smd.AddressU = smd.AddressV = smd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        smd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        smd.MinLOD = 0.0f;
        smd.MaxLOD = D3D11_FLOAT32_MAX;
        return SUCCEEDED(device->CreateSamplerState(&smd, ppSamp));
    }

}

// Body-part classification shared by the skeleton + aim hitbox builders.
// Roblox avatar exports split the body into many small groups; folding them
// into coarse limbs here keeps overlays from picking a single (often wrong,
// e.g. foot-sized) part.
// returns: 0 head, 1 torso, 2 left arm, 3 right arm, 4 left leg, 5 right leg
static int ClassifyBodyPart(const ModelPartAABB& p, float bx0, float by0, float by1,
                            float cx, float h, float w)
{
    const float pcx = (p.min[0] + p.max[0]) * 0.5f;
    const float pcy = (p.min[1] + p.max[1]) * 0.5f;
    const float psy = p.max[1] - p.min[1];
    const float psx = p.max[0] - p.min[0];

    // head: top region, small vertical span
    if (pcy > by0 + h * 0.78f && psy < h * 0.45f)
        return 0;

    // arms: outside the central x-band (catches hands even when they hang low)
    if (std::fabs(pcx - cx) > w * 0.30f && psx < w * 0.60f)
        return pcx < cx ? 2 : 3;

    // legs: everything below the torso that is still central
    if (pcy < by0 + h * 0.58f)
        return pcx < cx ? 4 : 5;

    return 1; // torso
}

void PreviewRenderer::BuildR6SkeletonFromParts()
{
    m_SkelSegCount = 0;

    const float bx0 = m_BodyMin[0], bx1 = m_BodyMax[0];
    const float by0 = m_BodyMin[1], by1 = m_BodyMax[1];
    const float cx = (bx0 + bx1) * 0.5f;
    const float h = (std::max)(0.01f, by1 - by0);
    const float w = (std::max)(0.01f, bx1 - bx0);

    // merge every part that belongs to the same limb into one AABB: Roblox
    // avatar exports (R15) have separate groups per limb segment (upper arm,
    // lower arm, hand), and a "last match wins" heuristic ends up picking the
    // feet for the legs (invisible foot-stub bones) and the hands wrong.
    // Folding each limb gives one full-length bone from shoulder/hip to
    // hand/foot.
    ModelPartAABB limbs[6]{}; // 0 head, 1 torso, 2 lArm, 3 rArm, 4 lLeg, 5 rLeg
    for (const auto& p : m_BodyParts)
    {
        if (!p.valid) continue;
        const int t = ClassifyBodyPart(p, bx0, by0, by1, cx, h, w);
        if (t < 0 || t >= 6) continue;
        if (!limbs[t].valid) { limbs[t] = p; limbs[t].valid = true; }
        else
        {
            for (int i = 0; i < 3; ++i)
            {
                limbs[t].min[i] = (std::min)(limbs[t].min[i], p.min[i]);
                limbs[t].max[i] = (std::max)(limbs[t].max[i], p.max[i]);
            }
        }
    }

    auto add = [&](float ax, float ay, float az, float bx, float by, float bz) {
        if (m_SkelSegCount >= MaxSkelSegs) return;
        m_SkelSeg[m_SkelSegCount][0][0] = ax;
        m_SkelSeg[m_SkelSegCount][0][1] = ay;
        m_SkelSeg[m_SkelSegCount][0][2] = az;
        m_SkelSeg[m_SkelSegCount][1][0] = bx;
        m_SkelSeg[m_SkelSegCount][1][1] = by;
        m_SkelSeg[m_SkelSegCount][1][2] = bz;
        ++m_SkelSegCount;
    };

    auto axis = [](const ModelPartAABB& p, float top[3], float bot[3]) {
        const float x = (p.min[0] + p.max[0]) * 0.5f;
        const float z = (p.min[2] + p.max[2]) * 0.5f;
        top[0] = x; top[1] = p.max[1]; top[2] = z;
        bot[0] = x; bot[1] = p.min[1]; bot[2] = z;
    };

    auto lerp3 = [](const float a[3], const float b[3], float t, float o[3]) {
        o[0] = a[0] + (b[0] - a[0]) * t;
        o[1] = a[1] + (b[1] - a[1]) * t;
        o[2] = a[2] + (b[2] - a[2]) * t;
    };

    if (!limbs[1].valid) return;

    float torso_top[3], torso_bot[3], shoulder_c[3];
    axis(limbs[1], torso_top, torso_bot);
    lerp3(torso_top, torso_bot, 0.18f, shoulder_c);
    add(shoulder_c[0], shoulder_c[1], shoulder_c[2],
        torso_bot[0], torso_bot[1], torso_bot[2]);

    auto arm_bones = [&](const ModelPartAABB& arm) {
        const float ax = (arm.min[0] + arm.max[0]) * 0.5f;
        const float az = (arm.min[2] + arm.max[2]) * 0.5f;
        const float elbow[3] = { ax, arm.max[1], az }; // joint near the shoulder
        const float hand[3]  = { ax, arm.min[1], az }; // lowest arm part = hand
        add(shoulder_c[0], shoulder_c[1], shoulder_c[2], elbow[0], elbow[1], elbow[2]);
        add(elbow[0], elbow[1], elbow[2], hand[0], hand[1], hand[2]);
    };
    if (limbs[2].valid) arm_bones(limbs[2]);
    if (limbs[3].valid) arm_bones(limbs[3]);

    auto leg_bones = [&](const ModelPartAABB& leg) {
        const float ax = (leg.min[0] + leg.max[0]) * 0.5f;
        const float az = (leg.min[2] + leg.max[2]) * 0.5f;
        const float hip[3]   = { ax, leg.max[1], az }; // top of the merged leg
        const float ankle[3] = { ax, leg.min[1], az }; // foot
        const float knee[3]  = { ax,
            leg.min[1] + (leg.max[1] - leg.min[1]) * 0.55f, az };
        add(hip[0], hip[1], hip[2], knee[0], knee[1], knee[2]);
        add(knee[0], knee[1], knee[2], ankle[0], ankle[1], ankle[2]);
    };
    if (limbs[4].valid) leg_bones(limbs[4]);
    if (limbs[5].valid) leg_bones(limbs[5]);
}
bool PreviewRenderer::ApplyLoadedModel(LoadedModel& model)
{
    std::vector<ModelVertex> combined = model.vertices;
    m_BodyVertCount = (unsigned int)model.vertices.size();
    m_WingVertCount = (unsigned int)model.wing_vertices.size();
    if (!model.wing_vertices.empty())
        combined.insert(combined.end(), model.wing_vertices.begin(), model.wing_vertices.end());

    unsigned int total = 0;
    if (!UploadVerts(m_Device, combined, &m_VB, total)) return false;
    if (m_BodyVertCount == 0 && m_WingVertCount > 0) {
        m_BodyVertCount = 0; // â•¤Ã©â•¨â•›â•¨â•—â•¤Ã®â•¨â•‘â•¨â•› â•¨â•‘â•¤Ã‡â•¤Ã¯â•¨â•—â•¤Ã®â•¤Ã…, â•¨â•›â•¨â•‘
    }

    m_ModelScale = model.scale;
    for (int i = 0; i < 3; ++i) {
        m_ModelCenter[i] = model.center[i];
        m_RawMin[i] = model.raw_min[i];
        m_RawMax[i] = model.raw_max[i];
        m_BodyMin[i] = model.body_min[i];
        m_BodyMax[i] = model.body_max[i];
        m_BoxMin[i] = model.box_min[i];
        m_BoxMax[i] = model.box_max[i];
    }

    m_FrameCenter[0] = (m_BoxMin[0] + m_BoxMax[0]) * 0.5f;
    m_FrameCenter[1] = (m_BoxMin[1] + m_BoxMax[1]) * 0.5f;
    m_FrameCenter[2] = (m_BoxMin[2] + m_BoxMax[2]) * 0.5f;
    {
        const float ext[3] = {
            m_BoxMax[0] - m_BoxMin[0],
            m_BoxMax[1] - m_BoxMin[1],
            m_BoxMax[2] - m_BoxMin[2]
        };
        const float maxExt = (std::max)({ ext[0], ext[1], ext[2] });

        m_FrameScale = maxExt > 0.0f ? (0.82f / maxExt) : 1.0f;
    }

    m_UniquePos = std::move(model.positions);
    m_BodyPos = std::move(model.body_positions);
    m_BoxPos = std::move(model.box_positions);
    m_BodyParts = std::move(model.body_parts);
    m_BoxParts = std::move(model.box_parts);

    BuildR6SkeletonFromParts();
    BuildAimPartBoxes();

    m_SceneDirty = true;
    m_CachedBoundsValid = false;
    return true;
}

void PreviewRenderer::BuildAimPartBoxes()
{
    for (int i = 0; i < AimPartCount; ++i)
        m_AimParts[i] = {};

    const float bx0 = m_BodyMin[0], bx1 = m_BodyMax[0];
    const float by0 = m_BodyMin[1], by1 = m_BodyMax[1];
    const float cx = (bx0 + bx1) * 0.5f;
    const float h = (std::max)(0.01f, by1 - by0);
    const float w = (std::max)(0.01f, bx1 - bx0);

    // same limb-merging as the skeleton, so the aim dots/boxes land on the
    // real head / torso / hands / feet of the (R15) avatar
    ModelPartAABB limbs[6]{};
    for (const auto& p : m_BodyParts)
    {
        if (!p.valid) continue;
        const int t = ClassifyBodyPart(p, bx0, by0, by1, cx, h, w);
        if (t < 0 || t >= 6) continue;
        if (!limbs[t].valid) { limbs[t] = p; limbs[t].valid = true; }
        else
        {
            for (int i = 0; i < 3; ++i)
            {
                limbs[t].min[i] = (std::min)(limbs[t].min[i], p.min[i]);
                limbs[t].max[i] = (std::max)(limbs[t].max[i], p.max[i]);
            }
        }
    }

    auto copy = [](AimPartAABB& dst, const ModelPartAABB& src) {
        for (int i = 0; i < 3; ++i) {
            dst.min[i] = src.min[i];
            dst.max[i] = src.max[i];
        }
        dst.valid = true;
    };

    if (limbs[0].valid) copy(m_AimParts[0], limbs[0]); // head
    if (limbs[1].valid) copy(m_AimParts[1], limbs[1]); // upper torso
    if (limbs[2].valid) copy(m_AimParts[4], limbs[2]); // left hand
    if (limbs[3].valid) copy(m_AimParts[5], limbs[3]); // right hand
    if (limbs[4].valid) copy(m_AimParts[6], limbs[4]); // left foot
    if (limbs[5].valid) copy(m_AimParts[7], limbs[5]); // right foot
}
bool PreviewRenderer::LoadModel(const std::string& obj_path)
{
    LoadedModel model;
    if (!LoadOBJ(obj_path, model)) return false;
    return ApplyLoadedModel(model);
}

bool PreviewRenderer::LoadModelFromMemory(const char* obj_src, std::size_t obj_len)
{
    LoadedModel model;
    if (!LoadOBJFromMemory(obj_src, obj_len, model)) return false;
    return ApplyLoadedModel(model);
}

bool PreviewRenderer::LoadTexture(const std::string& tex_path)
{
    int w = 0, h = 0, n = 0;
    stbi_uc* data = stbi_load(tex_path.c_str(), &w, &h, &n, 4);
    if (!data) return false;
    const bool ok = UploadPng(m_Device, data, w, h, &m_TexRes, &m_TexSRV, &m_Sampler);
    stbi_image_free(data);
    if (ok) m_SceneDirty = true;
    return ok;
}

bool PreviewRenderer::LoadTextureFromMemory(const unsigned char* png, std::size_t png_len)
{
    if (!png || png_len == 0) return false;
    int w = 0, h = 0, n = 0;
    stbi_uc* data = stbi_load_from_memory(png, (int)png_len, &w, &h, &n, 4);
    if (!data) return false;
    const bool ok = UploadPng(m_Device, data, w, h, &m_TexRes, &m_TexSRV, &m_Sampler);
    stbi_image_free(data);
    if (ok) m_SceneDirty = true;
    return ok;
}

// ---------------------------------------------------------------------------
// Avatar (multi-material) support
// ---------------------------------------------------------------------------
namespace {

struct OVec3 { float x = 0.f, y = 0.f, z = 0.f; };
struct OVec2 { float x = 0.f, y = 0.f; };

struct OFaceVtx {
    int v = 0, vt = 0, vn = 0;
};

struct SubMeshData {
    std::string name;
    std::vector<ModelVertex> verts;
};

OFaceVtx ParseObjFaceVtx(const std::string& token)
{
    OFaceVtx fv;
    const size_t first = token.find('/');
    if (first == std::string::npos)
    {
        fv.v = std::stoi(token);
        return fv;
    }
    fv.v = std::stoi(token.substr(0, first));
    const size_t second = token.find('/', first + 1);
    if (second == std::string::npos)
    {
        fv.vt = std::stoi(token.substr(first + 1));
    }
    else
    {
        if (second > first + 1)
            fv.vt = std::stoi(token.substr(first + 1, second - first - 1));
        fv.vn = std::stoi(token.substr(second + 1));
    }
    return fv;
}

// Splits a Roblox avatar OBJ into one vertex list per material ("usemtl").
// Positions/normals/UVs kept as-is (same as the standalone avatar-preview
// tool - Roblox OBJ UVs are already top-origin for D3D).
std::vector<SubMeshData> ParseSubMeshes(const std::string& obj)
{
    std::vector<OVec3> pos;
    std::vector<OVec2> uv;
    std::vector<OVec3> nrm;

    std::map<std::string, SubMeshData> meshes;
    std::string cur = "Default";

    std::istringstream stream(obj);
    std::string line;
    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream ls(line);
        std::string cmd;
        if (!(ls >> cmd))
            continue;

        if (cmd == "v")
        {
            OVec3 p;
            ls >> p.x >> p.y >> p.z;
            pos.push_back(p);
        }
        else if (cmd == "vt")
        {
            OVec2 t;
            ls >> t.x >> t.y;
            uv.push_back(t);
        }
        else if (cmd == "vn")
        {
            OVec3 n;
            ls >> n.x >> n.y >> n.z;
            nrm.push_back(n);
        }
        else if (cmd == "usemtl")
        {
            ls >> cur;
        }
        else if (cmd == "f")
        {
            std::vector<OFaceVtx> fv;
            std::string tok;
            while (ls >> tok)
                fv.push_back(ParseObjFaceVtx(tok));
            if (fv.size() < 3)
                continue;

            SubMeshData& mesh = meshes[cur];
            auto emit = [&](const OFaceVtx& f) {
                ModelVertex mv{};
                mv.nx = 0.f; mv.ny = 1.f; mv.nz = 0.f;
                if (f.v > 0 && f.v <= (int)pos.size())
                {
                    mv.x = pos[f.v - 1].x;
                    mv.y = pos[f.v - 1].y;
                    mv.z = pos[f.v - 1].z;
                }
                if (f.vt > 0 && f.vt <= (int)uv.size())
                {
                    mv.u = uv[f.vt - 1].x;
                    // OBJ vt v is top-down (Roblox exporter), while the shader
                    // samples with v=0 at the top. The standalone avatar-preview
                    // tool does this flip in its vertex shader
                    // (output.uv = float2(uv.x, 1.0f - uv.y)); we do the same
                    // by baking it into the vertex here so the shared shader
                    // (also used by the premade model) stays untouched.
                    mv.v = 1.f - uv[f.vt - 1].y;
                }
                if (f.vn > 0 && f.vn <= (int)nrm.size())
                {
                    mv.nx = nrm[f.vn - 1].x;
                    mv.ny = nrm[f.vn - 1].y;
                    mv.nz = nrm[f.vn - 1].z;
                }
                mesh.verts.push_back(mv);
            };

            for (size_t i = 1; i + 1 < fv.size(); ++i)
            {
                emit(fv[0]);
                emit(fv[i]);
                emit(fv[i + 1]);
            }
        }
    }

    std::vector<SubMeshData> out;
    out.reserve(meshes.size());
    for (auto& kv : meshes)
    {
        if (!kv.second.verts.empty())
        {
            // the material name lives in the map key - it must be copied onto
            // the submesh, otherwise LoadAvatar's material lookup (which
            // matches by name against the MTL materials) never succeeds and
            // the whole avatar renders untextured
            kv.second.name = kv.first;
            out.push_back(std::move(kv.second));
        }
    }
    return out;
}

} // namespace

bool PreviewRenderer::EnsureWhiteTexture()
{
    if (m_WhiteSRV)
        return true;

    unsigned char px[4]{ 0xFF, 0xFF, 0xFF, 0xFF };
    D3D11_TEXTURE2D_DESC td{};
    td.Width = 1;
    td.Height = 1;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = px;
    sd.SysMemPitch = 4;

    ID3D11Texture2D* tex = nullptr;
    if (FAILED(m_Device->CreateTexture2D(&td, &sd, &tex)))
        return false;
    if (FAILED(m_Device->CreateShaderResourceView(tex, nullptr, &m_WhiteSRV)))
    {
        tex->Release();
        m_WhiteSRV = nullptr;
        return false;
    }
    tex->Release();
    return true;
}

void PreviewRenderer::ClearAvatar()
{
    for (auto& sm : m_SubMeshes)
    {
        if (sm.vb)  sm.vb->Release();
        if (sm.srv) sm.srv->Release();
    }
    m_SubMeshes.clear();
}

bool PreviewRenderer::LoadAvatar(const std::string& obj_source,
                                 const std::vector<PreviewMaterial>& materials)
{
    if (!m_Device || !m_Ctx)
        return false;
    ClearAvatar();

    // geometry (body parts / skeleton / aim dots / bounds) - the standard
    // loader ignores "usemtl" lines and keeps the whole mesh in one buffer;
    // overlays keep working exactly as with the premade model.
    if (!LoadModelFromMemory(obj_source.data(), obj_source.size()))
        return false;

    if (!EnsureWhiteTexture())
        return false;

    const std::vector<SubMeshData> sub = [&]() {
        try
        {
            return ParseSubMeshes(obj_source);
        }
        catch (...)
        {
            return std::vector<SubMeshData>{};
        }
    }();
    if (sub.empty())
    {
        ClearAvatar();
        return false;
    }

    for (const auto& s : sub)
    {
        const PreviewMaterial* mat = nullptr;
        for (const auto& m : materials)
        {
            if (m.name == s.name)
            {
                mat = &m;
                break;
            }
        }

        SubMesh sm;
        sm.kd[0] = sm.kd[1] = sm.kd[2] = 1.f;

        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = (UINT)(s.verts.size() * sizeof(ModelVertex));
        bd.Usage = D3D11_USAGE_IMMUTABLE;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vd{};
        vd.pSysMem = s.verts.data();
        if (FAILED(m_Device->CreateBuffer(&bd, &vd, &sm.vb)))
            continue;
        sm.count = (UINT)s.verts.size();

        if (mat && !mat->png.empty())
        {
            int w = 0, h = 0, n = 0;
            stbi_uc* px = stbi_load_from_memory(mat->png.data(), (int)mat->png.size(),
                                                &w, &h, &n, 4);
            if (px)
            {
                D3D11_TEXTURE2D_DESC td{};
                td.Width = (UINT)w;
                td.Height = (UINT)h;
                td.MipLevels = 1;
                td.ArraySize = 1;
                td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                td.SampleDesc.Count = 1;
                td.Usage = D3D11_USAGE_DEFAULT;
                td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

                D3D11_SUBRESOURCE_DATA tsd{};
                tsd.pSysMem = px;
                tsd.SysMemPitch = (UINT)(w * 4);

                ID3D11Texture2D* tex = nullptr;
                if (SUCCEEDED(m_Device->CreateTexture2D(&td, &tsd, &tex)))
                {
                    if (FAILED(m_Device->CreateShaderResourceView(tex, nullptr, &sm.srv)))
                        sm.srv = nullptr;
                    tex->Release();
                }
                stbi_image_free(px);
            }
            // texture bytes present but failed to decode/upload (bad download,
            // gpu limits, ...) -> fall back to the material's Kd colour instead
            // of rendering pure white
            if (!sm.srv)
            {
                sm.kd[0] = mat->kd[0];
                sm.kd[1] = mat->kd[1];
                sm.kd[2] = mat->kd[2];
            }
        }
        else if (mat)
        {
            sm.kd[0] = mat->kd[0];
            sm.kd[1] = mat->kd[1];
            sm.kd[2] = mat->kd[2];
        }

        m_SubMeshes.emplace_back(sm);
    }

    m_SceneDirty = true;
    return !m_SubMeshes.empty();
}

void PreviewRenderer::AddRotationDelta(float dyaw, float dpitch)
{
	m_ManualYaw += dyaw;
	m_ManualPitch += dpitch;
	if (m_ManualPitch > 1.2f) m_ManualPitch = 1.2f;
	if (m_ManualPitch < -1.2f) m_ManualPitch = -1.2f;
	m_SceneDirty = true;
}

void PreviewRenderer::AddZoom(float delta)
{
	m_Zoom += delta;
	if (m_Zoom < 0.45f) m_Zoom = 0.45f;
	if (m_Zoom > 2.8f) m_Zoom = 2.8f;
	m_SceneDirty = true;
}

float PreviewRenderer::WingAlpha() const
{
	// fade in, hold, out, â•¨â”â•¨â–‘â•¤Ã¢â•¨â•–â•¨â–‘
	float fade_in = 1.15f;
	float hold = 1.60f;
	float fade_out = 1.15f;
	float pause = 0.90f;
	float period = fade_in + hold + fade_out + pause;

	float t = std::fmod(m_WingAnimTime, period);
	if (t < 0.f)
		t += period;

	auto smooth = [](float x)
	{
		if (x < 0.f) x = 0.f;
		if (x > 1.f) x = 1.f;
		return x * x * (3.f - 2.f * x);
	};

	if (t < fade_in)
		return smooth(t / fade_in);
	t -= fade_in;

	if (t < hold)
		return 1.f;
	t -= hold;

	if (t < fade_out)
		return 1.f - smooth(t / fade_out);

	(void)pause;
	return 0.f;
}

void PreviewRenderer::Update(float dt)
{
	if (!m_Ready || !m_VB)
		return;

	if (m_SpinPauseRemaining > 0.0f)
		m_SpinPauseRemaining -= dt;

	if (m_AutoSpin && m_SpinPauseRemaining <= 0.0f)
	{
		m_SpinAccum += dt;
		if (m_SpinAccum >= 1.0f / 20.0f)
		{
			m_Angle += m_SpinAccum * 0.35f;
			m_SpinAccum = 0.0f;
			m_SceneDirty = true;
		}
	}

	else
	{
		m_SpinAccum = 0.0f;
	}

	if (m_WingVertCount > 0)
	{
		m_WingAnimTime += dt;
		m_SceneDirty = true;
	}

	if (!m_SceneDirty)
		return;
	m_SceneDirty = false;

	float totalYaw = m_Angle + m_ManualYaw;
	float totalPitch = m_ManualPitch;
	float camDist = 2.35f / m_Zoom;

	XMMATRIX world = XMMatrixTranslation(-m_FrameCenter[0], -m_FrameCenter[1], -m_FrameCenter[2])
	               * XMMatrixScaling(m_FrameScale, m_FrameScale, m_FrameScale)
	               * XMMatrixRotationX(totalPitch)
	               * XMMatrixRotationY(totalYaw);

	float aspect = (float)m_Width / (float)m_Height;
	XMMATRIX view = XMMatrixLookAtLH(
		XMVectorSet(0.0f, 0.02f, -camDist, 1.0f),
		XMVectorSet(0.0f, 0.02f, 0.0f, 1.0f),
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(40.0f), aspect, 0.01f, 100.0f);

	XMMATRIX cpuMVP = world * view * proj;
	XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(m_LastMVP), cpuMVP);
	m_LastMVPValid = true;
	m_CachedBoundsValid = false;

	auto upload_cb = [&](float opacity, float dr = 1.f, float dg = 1.f, float db = 1.f)
	{
		XMMATRIX gpuMVP = XMMatrixTranspose(cpuMVP);
		XMMATRIX gpuWorld = XMMatrixTranspose(world);
		D3D11_MAPPED_SUBRESOURCE ms{};
		if (SUCCEEDED(m_Ctx->Map(m_CB, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms)))
		{
			float* dst = (float*)ms.pData;
			memcpy(dst, &gpuMVP, 64);
			memcpy(dst + 16, &gpuWorld, 64);
			dst[32] = opacity;
			dst[33] = 0.0f;
			dst[34] = 0.0f;
			dst[35] = 0.0f;
			dst[36] = dr;
			dst[37] = dg;
			dst[38] = db;
			dst[39] = 1.0f;
			m_Ctx->Unmap(m_CB, 0);
		}
	};

    ID3D11RenderTargetView* prevRTV = nullptr;
    ID3D11DepthStencilView* prevDSV = nullptr;
    ID3D11RasterizerState* prevRS = nullptr;
    ID3D11DepthStencilState* prevDS = nullptr;
    UINT prevDSRef = 0;
    float prevBF[4]{}; UINT prevSM = 0;
    ID3D11BlendState* prevBS = nullptr;
    UINT numVP = 1; D3D11_VIEWPORT prevVP{};

    m_Ctx->OMGetRenderTargets(1, &prevRTV, &prevDSV);
    m_Ctx->RSGetState(&prevRS);
    m_Ctx->OMGetDepthStencilState(&prevDS, &prevDSRef);
    m_Ctx->OMGetBlendState(&prevBS, prevBF, &prevSM);
    m_Ctx->RSGetViewports(&numVP, &prevVP);

    m_Ctx->OMSetRenderTargets(1, &m_RTV, m_DSV);
	// Ð¿Ñ€Ð¾Ð·Ñ€Ð°Ñ‡Ð½Ñ‹Ð¹ clear â€” Ñ„Ð¾Ð½ Ð¸Ð´Ñ‘Ñ‚ Ð¸Ð· child Ð¼ÐµÐ½ÑŽ, Ð½Ðµ Ñ‡Ñ‘Ñ€Ð½Ñ‹Ð¹ ÐºÐ²Ð°Ð´Ñ€Ð°Ñ‚
    const float clear_bg[4] = { 0.f, 0.f, 0.f, 0.f };
    m_Ctx->ClearRenderTargetView(m_RTV, clear_bg);
    m_Ctx->ClearDepthStencilView(m_DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

    D3D11_VIEWPORT vp{};
    vp.Width = (float)m_Width; vp.Height = (float)m_Height; vp.MaxDepth = 1.0f;
    m_Ctx->RSSetViewports(1, &vp);
    m_Ctx->RSSetState(m_RS);

    UINT stride = sizeof(ModelVertex), offset = 0;
    m_Ctx->IASetInputLayout(m_IL);
    m_Ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_Ctx->VSSetShader(m_VS, nullptr, 0);
    m_Ctx->VSSetConstantBuffers(0, 1, &m_CB);
    m_Ctx->PSSetShader(m_PS, nullptr, 0);
    m_Ctx->PSSetConstantBuffers(0, 1, &m_CB);
    if (m_Sampler) m_Ctx->PSSetSamplers(0, 1, &m_Sampler);

    float bf[4]{};
    m_Ctx->OMSetDepthStencilState(m_DSState, 0);
    m_Ctx->OMSetBlendState(m_BlendOpaque, bf, 0xFFFFFFFF);

    if (!m_SubMeshes.empty())
    {
        // avatar: one textured draw per material (real local-player model)
        ID3D11ShaderResourceView* white = m_WhiteSRV ? m_WhiteSRV : nullptr;
        for (const auto& sm : m_SubMeshes)
        {
            ID3D11ShaderResourceView* srv = sm.srv ? sm.srv : white;
            if (srv) m_Ctx->PSSetShaderResources(0, 1, &srv);
            m_Ctx->IASetVertexBuffers(0, 1, &sm.vb, &stride, &offset);
            upload_cb(1.0f, sm.kd[0], sm.kd[1], sm.kd[2]);
            m_Ctx->Draw(sm.count, 0);
        }
        ID3D11ShaderResourceView* nullSrv = nullptr;
        m_Ctx->PSSetShaderResources(0, 1, &nullSrv);
    }
    else
    {
        m_Ctx->IASetVertexBuffers(0, 1, &m_VB, &stride, &offset);
        if (m_TexSRV) m_Ctx->PSSetShaderResources(0, 1, &m_TexSRV);

        upload_cb(1.0f);
        if (m_BodyVertCount > 0)
            m_Ctx->Draw(m_BodyVertCount, 0);

        float wingA = WingAlpha();
        if (m_WingVertCount > 0 && wingA > 0.001f)
        {
            upload_cb(wingA);
            m_Ctx->OMSetDepthStencilState(m_DSNoWrite, 0);
            m_Ctx->OMSetBlendState(m_BlendAlpha, bf, 0xFFFFFFFF);
            m_Ctx->Draw(m_WingVertCount, m_BodyVertCount);
        }
    }

	if (m_SampleCount > 1 && m_ResolveTex)
		m_Ctx->ResolveSubresource(m_ResolveTex, 0, m_RTTex, 0, DXGI_FORMAT_R8G8B8A8_UNORM);

	else if (m_ResolveTex)
		m_Ctx->CopyResource(m_ResolveTex, m_RTTex);

    m_Ctx->OMSetRenderTargets(1, &prevRTV, prevDSV);
    m_Ctx->RSSetState(prevRS);
    m_Ctx->OMSetDepthStencilState(prevDS, prevDSRef);
    m_Ctx->OMSetBlendState(prevBS, prevBF, prevSM);
    m_Ctx->RSSetViewports(1, &prevVP);
    if (prevRTV) prevRTV->Release();
    if (prevDSV) prevDSV->Release();
    if (prevRS) prevRS->Release();
    if (prevDS) prevDS->Release();
    if (prevBS) prevBS->Release();
}

static bool ProjectMVP(const float* M, float x, float y, float z, float& u, float& v)
{
    const float clip_x = x * M[0] + y * M[4] + z * M[8]  + M[12];
    const float clip_y = x * M[1] + y * M[5] + z * M[9]  + M[13];
    const float clip_w = x * M[3] + y * M[7] + z * M[11] + M[15];
    if (clip_w <= 1e-5f) return false;
    const float inv = 1.0f / clip_w;
    u = clip_x * inv * 0.5f + 0.5f;
    v = -clip_y * inv * 0.5f + 0.5f;
    return true;
}

bool PreviewRenderer::GetProjectedUVBounds(float& u0, float& v0, float& u1, float& v1) const
{
    if (!m_LastMVPValid) return false;
    if (m_CachedBoundsValid) {
        u0 = m_CachedU0; v0 = m_CachedV0; u1 = m_CachedU1; v1 = m_CachedV1;
        return true;
    }

    float minU = 1e9f, minV = 1e9f, maxU = -1e9f, maxV = -1e9f;
    bool any = false;
    const float* M = m_LastMVP;

    auto expand = [&](float x, float y, float z) {
        float u, v;
        if (!ProjectMVP(M, x, y, z, u, v)) return;
        minU = (std::min)(minU, u); maxU = (std::max)(maxU, u);
        minV = (std::min)(minV, v); maxV = (std::max)(maxV, v);
        any = true;
    };

    for (const auto& p : m_BodyParts) {
        if (!p.valid) continue;
        for (int i = 0; i < 8; ++i) {
            expand((i & 1) ? p.max[0] : p.min[0],
                   (i & 2) ? p.max[1] : p.min[1],
                   (i & 4) ? p.max[2] : p.min[2]);
        }
    }

    const std::vector<float>& pos =
        !m_BoxPos.empty() ? m_BoxPos :
        (!m_BodyPos.empty() ? m_BodyPos : m_UniquePos);
    for (size_t i = 0; i + 2 < pos.size(); i += 3)
        expand(pos[i], pos[i + 1], pos[i + 2]);

    if (!any) return false;

    m_CachedU0 = u0 = minU; m_CachedV0 = v0 = minV;
    m_CachedU1 = u1 = maxU; m_CachedV1 = v1 = maxV;
    m_CachedBoundsValid = true;
    return true;
}

bool PreviewRenderer::GetProjectedPartBoxes(
    std::vector<std::array<std::pair<float, float>, 8>>& out) const
{
    out.clear();
    if (!m_LastMVPValid || m_BodyParts.empty()) return false;

    const float* M = m_LastMVP;
    out.reserve(m_BodyParts.size());
    for (const auto& p : m_BodyParts) {
        if (!p.valid) continue;
        std::array<std::pair<float, float>, 8> box{};
        bool ok = true;
        for (int i = 0; i < 8; ++i) {

            const float x = (i & 4) ? p.max[0] : p.min[0];
            const float y = (i & 2) ? p.max[1] : p.min[1];
            const float z = (i & 1) ? p.max[2] : p.min[2];
            float u, v;
            if (!ProjectMVP(M, x, y, z, u, v)) { ok = false; break; }
            box[i] = { u, v };
        }
        if (ok) out.push_back(box);
    }
    return !out.empty();
}

bool PreviewRenderer::GetProjectedAimPartBoxes(
    std::vector<std::pair<int, std::array<std::pair<float, float>, 8>>>& out) const
{
    out.clear();
    if (!m_LastMVPValid) return false;

    const float* M = m_LastMVP;
    out.reserve(AimPartCount);
    for (int pi = 0; pi < AimPartCount; ++pi) {
        const AimPartAABB& p = m_AimParts[pi];
        if (!p.valid) continue;
        std::array<std::pair<float, float>, 8> box{};
        bool ok = true;
        for (int i = 0; i < 8; ++i) {
            const float x = (i & 4) ? p.max[0] : p.min[0];
            const float y = (i & 2) ? p.max[1] : p.min[1];
            const float z = (i & 1) ? p.max[2] : p.min[2];
            float u, v;
            if (!ProjectMVP(M, x, y, z, u, v)) { ok = false; break; }
            box[i] = { u, v };
        }
        if (ok) out.emplace_back(pi, box);
    }
    return !out.empty();
}

bool PreviewRenderer::GetProjectedAimPartCenters(
    std::vector<std::pair<int, std::pair<float, float>>>& out) const
{
    out.clear();
    if (!m_LastMVPValid) return false;

    const float* M = m_LastMVP;
    out.reserve(AimPartCount);
    for (int pi = 0; pi < AimPartCount; ++pi) {
        const AimPartAABB& p = m_AimParts[pi];
        if (!p.valid) continue;
        const float x = (p.min[0] + p.max[0]) * 0.5f;
        const float y = (p.min[1] + p.max[1]) * 0.5f;
        const float z = (p.min[2] + p.max[2]) * 0.5f;
        float u, v;
        if (!ProjectMVP(M, x, y, z, u, v)) continue;
        out.emplace_back(pi, std::make_pair(u, v));
    }
    return !out.empty();
}

bool PreviewRenderer::GetProjectedR6Skeleton(std::vector<float>& out_uv_segs) const
{
    out_uv_segs.clear();
    if (!m_LastMVPValid || m_SkelSegCount <= 0) return false;
    out_uv_segs.reserve((size_t)m_SkelSegCount * 4);
    for (int i = 0; i < m_SkelSegCount; ++i) {
        float u0, v0, u1, v1;
        if (!ProjectMVP(m_LastMVP,
                m_SkelSeg[i][0][0], m_SkelSeg[i][0][1], m_SkelSeg[i][0][2], u0, v0))
            continue;
        if (!ProjectMVP(m_LastMVP,
                m_SkelSeg[i][1][0], m_SkelSeg[i][1][1], m_SkelSeg[i][1][2], u1, v1))
            continue;
        out_uv_segs.push_back(u0);
        out_uv_segs.push_back(v0);
        out_uv_segs.push_back(u1);
        out_uv_segs.push_back(v1);
    }
    return out_uv_segs.size() >= 4;
}

void* PreviewRenderer::GetTextureID() const { return (void*)m_SRV; }

bool PreviewRenderer::ProjectPoint(float x, float y, float z, float& u, float& v) const
{
    if (!m_LastMVPValid)
        return false;
    return ProjectMVP(m_LastMVP, x, y, z, u, v);
}

bool PreviewRenderer::GetHeadCenter(float out[3]) const
{
    if (!m_AimParts[0].valid)
        return false;
    out[0] = (m_AimParts[0].min[0] + m_AimParts[0].max[0]) * 0.5f;
    out[1] = (m_AimParts[0].min[1] + m_AimParts[0].max[1]) * 0.5f;
    out[2] = (m_AimParts[0].min[2] + m_AimParts[0].max[2]) * 0.5f;
    return true;
}

bool PreviewRenderer::GetHeadRadius(float& r) const
{
    if (!m_AimParts[0].valid)
        return false;
    const float w = m_AimParts[0].max[0] - m_AimParts[0].min[0];
    const float d = m_AimParts[0].max[2] - m_AimParts[0].min[2];
    r = (std::max)(w, d) * 0.5f;
    return r > 0.001f;
}

void PreviewRenderer::Shutdown()
{
    auto rel = [](auto*& p) { if (p) { p->Release(); p = nullptr; } };
    
    ClearAvatar();
    rel(m_WhiteSRV);

    rel(m_VB); rel(m_CB); rel(m_IL); rel(m_VS); rel(m_PS);
    rel(m_TexSRV); rel(m_TexRes); rel(m_Sampler);
    rel(m_RS); rel(m_DSState); rel(m_DSNoWrite);
    rel(m_BlendOpaque); rel(m_BlendAlpha);
    rel(m_SRV); rel(m_RTV); rel(m_RTTex); rel(m_ResolveTex); rel(m_DSV); rel(m_DepthTex);
    m_BodyVertCount = 0;
    m_WingVertCount = 0;
    m_WingAnimTime = 0.0f;
    m_Ready = false;
    m_LastMVPValid = false;
}

}
