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

void PreviewRenderer::BuildR6SkeletonFromParts()
{
    m_SkelSegCount = 0;

    const float bx0 = m_BodyMin[0], bx1 = m_BodyMax[0];
    const float by0 = m_BodyMin[1], by1 = m_BodyMax[1];
    const float cx = (bx0 + bx1) * 0.5f;
    const float h = (std::max)(0.01f, by1 - by0);
    const float w = (std::max)(0.01f, bx1 - bx0);

    // эвристика по aabb, голова/руки/ноги
    const ModelPartAABB* torso = nullptr;
    const ModelPartAABB* lArm = nullptr;
    const ModelPartAABB* rArm = nullptr;
    const ModelPartAABB* lLeg = nullptr;
    const ModelPartAABB* rLeg = nullptr;

    for (const auto& p : m_BodyParts) {
        if (!p.valid) continue;
        const float pcx = (p.min[0] + p.max[0]) * 0.5f;
        const float pcy = (p.min[1] + p.max[1]) * 0.5f;
        const float psy = p.max[1] - p.min[1];
        const float psx = p.max[0] - p.min[0];

        if (pcy > by0 + h * 0.82f && psy < h * 0.45f)
            continue; // голову в скелет не кладём
        if (pcy < by0 + h * 0.45f) {
            if (pcx < cx) lLeg = &p; else rLeg = &p;
            continue;
        }
        if (std::fabs(pcx - cx) > w * 0.28f && psx < w * 0.55f) {
            if (pcx < cx) lArm = &p; else rArm = &p;
            continue;
        }
        if (std::fabs(pcx - cx) <= w * 0.35f && psy >= h * 0.25f)
            torso = &p;
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

    // чуть ниже верха торса
    float shoulder_drop = 0.18f;

    if (!torso) return;

    float torso_top[3], torso_bot[3], shoulder_c[3];
    axis(*torso, torso_top, torso_bot);
    lerp3(torso_top, torso_bot, shoulder_drop, shoulder_c);
    add(shoulder_c[0], shoulder_c[1], shoulder_c[2],
        torso_bot[0], torso_bot[1], torso_bot[2]);

    auto arm_bones = [&](const ModelPartAABB* arm) {
        if (!arm) return;

        const float ax = (arm->min[0] + arm->max[0]) * 0.5f;
        const float az = (arm->min[2] + arm->max[2]) * 0.5f;
        const float joint[3] = { ax, shoulder_c[1], az };
        const float bot[3]   = { ax, arm->min[1], az };
        add(shoulder_c[0], shoulder_c[1], shoulder_c[2], joint[0], joint[1], joint[2]);
        add(joint[0], joint[1], joint[2], bot[0], bot[1], bot[2]);
    };
    arm_bones(lArm);
    arm_bones(rArm);

    auto leg_bones = [&](const ModelPartAABB* leg) {
        if (!leg) return;
        float t[3], b[3];
        axis(*leg, t, b);
        add(torso_bot[0], torso_bot[1], torso_bot[2], t[0], t[1], t[2]);
        add(t[0], t[1], t[2], b[0], b[1], b[2]);
    };
    leg_bones(lLeg);
    leg_bones(rLeg);
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
        m_BodyVertCount = 0; // только крылья, ок
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

    const ModelPartAABB* head = nullptr;
    const ModelPartAABB* torso = nullptr;
    const ModelPartAABB* lArm = nullptr;
    const ModelPartAABB* rArm = nullptr;
    const ModelPartAABB* lLeg = nullptr;
    const ModelPartAABB* rLeg = nullptr;

    for (const auto& p : m_BodyParts) {
        if (!p.valid) continue;
        const float pcx = (p.min[0] + p.max[0]) * 0.5f;
        const float pcy = (p.min[1] + p.max[1]) * 0.5f;
        const float psy = p.max[1] - p.min[1];
        const float psx = p.max[0] - p.min[0];

        if (pcy > by0 + h * 0.82f && psy < h * 0.45f) {
            head = &p;
            continue;
        }
        if (pcy < by0 + h * 0.45f) {
            if (pcx < cx) lLeg = &p; else rLeg = &p;
            continue;
        }
        if (std::fabs(pcx - cx) > w * 0.28f && psx < w * 0.55f) {
            if (pcx < cx) lArm = &p; else rArm = &p;
            continue;
        }
        if (std::fabs(pcx - cx) <= w * 0.35f && psy >= h * 0.25f)
            torso = &p;
    }

    auto copy = [](AimPartAABB& dst, const ModelPartAABB& src) {
        for (int i = 0; i < 3; ++i) {
            dst.min[i] = src.min[i];
            dst.max[i] = src.max[i];
        }
        dst.valid = true;
    };

    if (head) copy(m_AimParts[0], *head);
    if (torso) copy(m_AimParts[1], *torso);
    if (lArm) copy(m_AimParts[4], *lArm);
    if (rArm) copy(m_AimParts[5], *rArm);
    if (lLeg) copy(m_AimParts[6], *lLeg);
    if (rLeg) copy(m_AimParts[7], *rLeg);
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

