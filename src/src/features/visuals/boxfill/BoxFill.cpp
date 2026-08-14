#include "BoxFill.h"

#include <stb_image.h>
#include <d3d11.h>

#include "imgs/ox_LB.h"
#include "imgs/ox_PT.h"
#include "imgs/ox_ST.h"
#include "imgs/ox_M2.h"
#include "imgs/ox_OB.h"
#include "imgs/ox_PF.h"
#include "imgs/ox_DB.h"
#include "imgs/ox_HR.h"
#include "imgs/ox_VP.h"
#include "imgs/ox_DK.h"
#include "imgs/ox_PA.h"
#include "imgs/ox_CB.h"
#include "imgs/ox_KO.h"
#include "imgs/ox_LO.h"
#include "imgs/ox_EL.h"
#include "imgs/ox_VK.h"
#include "imgs/ox_EG.h"
#include "imgs/ox_DI.h"
#include "imgs/ox_AK.h"
#include "imgs/ox_IC.h"
#include "imgs/ox_PR.h"
#include "imgs/ox_AG.h"
#include "imgs/ox_SK.h"
#include "imgs/ox_US.h"
#include "imgs/ox_SE.h"

namespace Cheat {
namespace Visuals {
namespace BoxFill {

struct Entry {
    ID3D11ShaderResourceView* srv = nullptr;
    int w = 0;
    int h = 0;
};

static Entry g_tex[COUNT]{};
static bool g_ok = false;
static bool g_remove_bg = false;
static ID3D11Device* g_dev = nullptr;

static bool LoadOne(ID3D11Device* device, const unsigned char* data, unsigned int size, Entry& out)
{
    if (!device || !data || size == 0)
        return false;

    int w = 0, h = 0, n = 0;
    unsigned char* pixels = stbi_load_from_memory(data, (int)size, &w, &h, &n, 4);
    if (!pixels)
        return false;

    if (g_remove_bg) {
        const int cnt = w * h;
        for (int i = 0; i < cnt; i++) {
            unsigned char* p = pixels + i * 4;
            if (p[0] > 240 && p[1] > 240 && p[2] > 240)
                p[3] = 0;
        }
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = (UINT)w;
    desc.Height = (UINT)h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sub{};
    sub.pSysMem = pixels;
    sub.SysMemPitch = (UINT)(w * 4);

    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = device->CreateTexture2D(&desc, &sub, &tex);
    stbi_image_free(pixels);
    if (FAILED(hr) || !tex)
        return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;

    ID3D11ShaderResourceView* srv = nullptr;
    hr = device->CreateShaderResourceView(tex, &srv_desc, &srv);
    tex->Release();
    if (FAILED(hr) || !srv)
        return false;

    out.srv = srv;
    out.w = w;
    out.h = h;
    return true;
}

void Init(ID3D11Device* device)
{
    if (g_ok || !device)
        return;

    g_dev = device;

    LoadOne(device, ox_LB, ox_LB_size, g_tex[LB]);
    LoadOne(device, ox_PT, ox_PT_size, g_tex[PT]);
    LoadOne(device, ox_ST, ox_ST_size, g_tex[ST]);
    LoadOne(device, ox_M2, ox_M2_size, g_tex[M2]);
    LoadOne(device, ox_OB, ox_OB_size, g_tex[OB]);
    LoadOne(device, ox_PF, ox_PF_size, g_tex[PF]);
    LoadOne(device, ox_DB, ox_DB_size, g_tex[DB]);
    LoadOne(device, ox_HR, ox_HR_size, g_tex[HR]);
    LoadOne(device, ox_VP, ox_VP_size, g_tex[VP]);
    LoadOne(device, ox_DK, ox_DK_size, g_tex[DK]);
    LoadOne(device, ox_PA, ox_PA_size, g_tex[PA]);
    LoadOne(device, ox_CB, ox_CB_size, g_tex[CB]);
    LoadOne(device, ox_KO, ox_KO_size, g_tex[KO]);
    LoadOne(device, ox_LO, ox_LO_size, g_tex[LO]);
    LoadOne(device, ox_EL, ox_EL_size, g_tex[EL]);
    LoadOne(device, ox_VK, ox_VK_size, g_tex[VK]);
    LoadOne(device, ox_EG, ox_EG_size, g_tex[EG]);
    LoadOne(device, ox_DI, ox_DI_size, g_tex[DI]);
    LoadOne(device, ox_AK, ox_AK_size, g_tex[AK]);
    LoadOne(device, ox_IC, ox_IC_size, g_tex[IC]);
    LoadOne(device, ox_PR, ox_PR_size, g_tex[PR]);
    LoadOne(device, ox_AG, ox_AG_size, g_tex[AG]);
    LoadOne(device, ox_SK, ox_SK_size, g_tex[SK]);
    LoadOne(device, ox_US, ox_US_size, g_tex[US]);
    LoadOne(device, ox_SE, ox_SE_size, g_tex[SE]);

    g_ok = true;
}

void Shutdown()
{
    for (int i = 0; i < COUNT; i++) {
        if (g_tex[i].srv) {
            g_tex[i].srv->Release();
            g_tex[i].srv = nullptr;
        }
        g_tex[i].w = 0;
        g_tex[i].h = 0;
    }

    g_ok = false;
    g_dev = nullptr;
}

void SetRemoveBg(ID3D11Device* device, bool remove_bg)
{
    if (g_remove_bg == remove_bg && g_ok)
        return;

    g_remove_bg = remove_bg;

    ID3D11Device* dev = device ? device : g_dev;
    if (!dev)
        return;

    Shutdown();
    g_dev = dev;
    Init(dev);
}

ID3D11ShaderResourceView* Get(int id)
{
    if (id < 0 || id >= COUNT)
        return nullptr;
    return g_tex[id].srv;
}

void Draw(ImDrawList* dl, int id, ImVec2 min, ImVec2 max, float alpha, bool no_plate)
{
    if (!dl)
        return;

    ID3D11ShaderResourceView* srv = Get(id);
    if (!srv)
        return;

    if (alpha < 0.f) alpha = 0.f;
    if (alpha > 1.f) alpha = 1.f;

    const int a8 = (int)(alpha * 255.f);

    if (!no_plate && !g_remove_bg) {
        if (id == LB || id == PT || id == SK)
            dl->AddRectFilled(min, max, IM_COL32(255, 255, 255, a8));
    }

    const ImU32 col = IM_COL32(255, 255, 255, a8);
    dl->AddImage((ImTextureID)srv, min, max, ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), col);
}

} // namespace BoxFill
} // namespace Visuals
} // namespace Cheat
