#include "pch.h"
#include "KillEffects.h"
#include "features/aim/Aim.h"
#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/player/PlayerHandler.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"
#include "core/roblox/math/Math.h"
#include "renderer/Renderer.h"
#include "app/Settings.h"
#include "features/misc/HitSounds.h"
#include "imgui.h"
#include "gui/colors/colors.h"
#include "gui/resources/fonts/fonts.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace Cheat {
    namespace Visuals {
        namespace KillEffects {

            namespace hit_chams_state {
                struct HitChamFx {
                    std::uint64_t addr = 0;
                    float age = 0.f;
                    float life = 0.55f;
                };
                std::vector<HitChamFx> list;
            }

            void NotifyHitChams(std::uint64_t player_addr)
            {
                if (!player_addr)
                {
                    return;
                }

                float life = g_Settings.esp.hit_chams_duration;
                if (life < 0.1f)
                {
                    life = 0.1f;
                }

                if (life > 3.f)
                {
                    life = 3.f;
                }

                for (auto& h : hit_chams_state::list)
                {
                    if (h.addr == player_addr)
                    {
                        h.age = 0.f;
                        h.life = life;
                        return;
                    }
                }

                hit_chams_state::HitChamFx hx{};
                hx.addr = player_addr;
                hx.age = 0.f;
                hx.life = life;
                hit_chams_state::list.push_back(hx);
                if (hit_chams_state::list.size() > 48)
                {
                    hit_chams_state::list.erase(hit_chams_state::list.begin());
                }
            }

            float HitChamsFade(std::uint64_t player_addr)
            {
                if (!player_addr || !g_Settings.esp.hit_chams)
                {
                    return 0.f;
                }

                for (const auto& h : hit_chams_state::list)
                {
                    if (h.addr != player_addr)
                    {
                        continue;
                    }

                    if (h.life <= 0.001f)
                    {
                        return 0.f;
                    }

                    float t = 1.f - (h.age / h.life);
                    if (t < 0.f)
                    {
                        t = 0.f;
                    }

                    if (t > 1.f)
                    {
                        t = 1.f;
                    }

                    return t;
                }

                return 0.f;
            }

            namespace {

                static const char* k_fx_names[] = {
                    "pixel burst",
                    "confetti",
                    "shatter",
                };
                static_assert(sizeof(k_fx_names) / sizeof(k_fx_names[0]) == EffectCount, "names");

                static const char* k_hitdata_names[] = {
                    "hit type",
                    "damage",
                    "health left",
                    "distance",
                    "part name",
                };
                static_assert(sizeof(k_hitdata_names) / sizeof(k_hitdata_names[0]) == Settings::HITDATA_MODE_COUNT, "hitdata");

                struct PendingKill {
                    std::uint64_t addr = 0;
                    Vector3       pos{};
                    double        click_at = 0.0;
                    bool          was_alive = false;
                };

                struct HitWatch {
                    std::uint64_t addr = 0;
                    float         last_hp = -1.0f;
                    double        keep_until = 0.0;
                    double        last_fx = 0.0;
                    int           part = 0;
                    Vector3       point{};
                };

                struct KillFx {
                    int     type = 0;
                    Vector3 origin{};
                    float   age = 0.0f;
                    float   life = 1.25f;
                    float   seed = 0.0f;
                };

                struct Marker {
                    Vector3 origin{};
                    float   age = 0.0f;
                    float   life = 0.55f;
                    bool    headshot = false;
                };

                struct HitText {
                    Vector3 origin{};
                    float   age = 0.0f;
                    float   life = 1.35f;
                    bool    headshot = false;
                    char    text[64]{};
                };

                std::vector<PendingKill> g_pending;
                std::vector<KillFx>      g_fx;
                std::vector<Marker>      g_markers;
                std::vector<HitText>     g_texts;
                HitWatch                 g_watch{};
                bool                     g_lmb_was = false;
                double                   g_lmb_at = -1.0;

                float Hash11(float n) {
                    const float s = std::sin(n * 127.1f) * 43758.5453f;
                    return s - std::floor(s);
                }

                ImU32 MulA(ImU32 c, float a)
                {
                    int na = (int)((float)((c >> IM_COL32_A_SHIFT) & 0xFF) * a + 0.5f);
                    if (na < 0) na = 0;
                    if (na > 255) na = 255;
                    return (c & ~IM_COL32_A_MASK) | ((ImU32)na << IM_COL32_A_SHIFT);
                }

                ImU32 LerpU32(ImU32 a, ImU32 b, float t)
                {
                    if (t < 0.f) t = 0.f;
                    if (t > 1.f) t = 1.f;
                    auto ch = [](ImU32 c, int sh) { return (int)((c >> sh) & 0xFF); };
                    int r = (int)(ch(a, IM_COL32_R_SHIFT) + (ch(b, IM_COL32_R_SHIFT) - ch(a, IM_COL32_R_SHIFT)) * t);
                    int g = (int)(ch(a, IM_COL32_G_SHIFT) + (ch(b, IM_COL32_G_SHIFT) - ch(a, IM_COL32_G_SHIFT)) * t);
                    int bl = (int)(ch(a, IM_COL32_B_SHIFT) + (ch(b, IM_COL32_B_SHIFT) - ch(a, IM_COL32_B_SHIFT)) * t);
                    int al = (int)(ch(a, IM_COL32_A_SHIFT) + (ch(b, IM_COL32_A_SHIFT) - ch(a, IM_COL32_A_SHIFT)) * t);
                    return IM_COL32(r, g, bl, al);
                }

                ImU32 Accent(float a = 1.0f) { return colors::accent_u32(a); }
                ImU32 TextCol(float a = 1.0f) { return colors::text_active_u32(a); }
                ImU32 SoftAccent(float a = 1.0f) {
                    return LerpU32(Accent(a), IM_COL32(255, 255, 255, (int)(255 * a)), 0.35f);
                }

                bool Project(const Matrix4x4& vm, const Vector2& vp,
                             float sx, float sy, const Vector3& p, ImVec2& out) {
                    const float w = p.x * vm.m[3][0] + p.y * vm.m[3][1] + p.z * vm.m[3][2] + vm.m[3][3];
                    if (w < 0.01f) return false;
                    const float inv = 1.0f / w;
                    const float x = (p.x * vm.m[0][0] + p.y * vm.m[0][1] + p.z * vm.m[0][2] + vm.m[0][3]) * inv;
                    const float y = (p.x * vm.m[1][0] + p.y * vm.m[1][1] + p.z * vm.m[1][2] + vm.m[1][3]) * inv;
                    out.x = ((vp.x * 0.5f) + (x * vp.x * 0.5f)) * sx;
                    out.y = ((vp.y * 0.5f) - (y * vp.y * 0.5f)) * sy;
                    return true;
                }

                float ScreenRadius(const Matrix4x4& vm, const Vector2& vp,
                                   float sx, float sy, const Vector3& origin, float world_r) {
                    ImVec2 a, b;
                    if (!Project(vm, vp, sx, sy, origin, a)) return 0.0f;
                    if (!Project(vm, vp, sx, sy, Vector3{ origin.x + world_r, origin.y, origin.z }, b))
                        return 40.0f;
                    const float dx = b.x - a.x, dy = b.y - a.y;
                    return (std::max)(4.0f, std::sqrt(dx * dx + dy * dy));
                }

                ImFont* GuiFont() {
                    ImFont* f = fonts::selected();
                    return f ? f : ImGui::GetFont();
                }

                const char* PartName(int part) {
                    switch (part) {
                        case Settings::AIM_HEAD:        return "Head";
                        case Settings::AIM_UPPER_TORSO: return "Upper Torso";
                        case Settings::AIM_LOWER_TORSO: return "Lower Torso";
                        case Settings::AIM_HRP:         return "Root";
                        case Settings::AIM_LEFT_HAND:   return "Left Hand";
                        case Settings::AIM_RIGHT_HAND:  return "Right Hand";
                        case Settings::AIM_LEFT_FOOT:   return "Left Foot";
                        case Settings::AIM_RIGHT_FOOT:  return "Right Foot";
                        default:                        return "Body";
                    }
                }

                bool IsHead(int part) { return part == Settings::AIM_HEAD; }
                bool IsBody(int part) {
                    return part == Settings::AIM_UPPER_TORSO
                        || part == Settings::AIM_LOWER_TORSO
                        || part == Settings::AIM_HRP;
                }

                const char* HitTypeLabel(int part) {
                    if (IsHead(part)) return "HEADSHOT";
                    if (IsBody(part)) return "BODY SHOT";
                    return "LIMB HIT";
                }

                bool LookupTarget(std::uint64_t addr, Vector3& out_pos, float& out_hp, bool& found) {
                    found = false;
                    out_hp = 0.0f;
                    bool ok = false;
                    PlayerHandler::ForEachPlayer([&](const PlayerCache& c) {
                        if (c.address != addr) return;
                        found = true;
                        if (c.humanoidRootPart && g_Memory.IsValid(c.humanoidRootPart->address)) {
                            out_pos = BasePart(c.humanoidRootPart->address).GetPosition();
                            ok = true;
                        }

                        else if (c.head && g_Memory.IsValid(c.head->address))
                        {
                            out_pos = BasePart(c.head->address).GetPosition();
                            ok = true;
                        }
                        if (c.humanoid && g_Memory.IsValid(c.humanoid->address))
                            out_hp = Humanoid(c.humanoid->address).GetHealth();
                    });
                    return ok;
                }

                void SpawnKill(int type, const Vector3& origin)
                {
                    KillFx fx;
                    fx.type = type;
                    fx.origin = origin;
                    fx.age = 0.0f;
                    fx.seed = (float)ImGui::GetTime() * 17.13f;

                    if (type == Confetti)
                        fx.life = 1.55f;

                    else if (type == Shatter)
                        fx.life = 1.20f;

                    else
                        fx.life = 1.30f; // PixelBurst

                    g_fx.push_back(fx);
                    if (g_fx.size() > 20)
                        g_fx.erase(g_fx.begin());
                }

                void SpawnMarker(const Vector3& origin, bool headshot)
                {
                    Marker m;
                    m.origin = origin;
                    m.age = 0.0f;
                    m.life = g_Settings.hitmarker.duration;
                    if (m.life < 0.2f)
                        m.life = 0.2f;
                    m.headshot = headshot;
                    g_markers.push_back(m);
                    if (g_markers.size() > 32)
                        g_markers.erase(g_markers.begin());
                }

                void FormatHitLine(char* buf, size_t n, int mode, int part,
                                   float damage, float hp_left, float dist) {
                    switch (mode) {
                        case Settings::HITDATA_DAMAGE:
                            std::snprintf(buf, n, "-%.0f", damage);
                            break;
                        case Settings::HITDATA_HEALTH:
                            std::snprintf(buf, n, "%.0f HP", (std::max)(0.0f, hp_left));
                            break;
                        case Settings::HITDATA_DISTANCE:
                            std::snprintf(buf, n, "%.0fm", dist);
                            break;
                        case Settings::HITDATA_PART:
                            std::snprintf(buf, n, "%s", PartName(part));
                            break;
                        case Settings::HITDATA_TYPE:
                        default:
                            std::snprintf(buf, n, "%s", HitTypeLabel(part));
                            break;
                    }
                }

                void SpawnHitTexts(const Vector3& origin, int part, float damage,
                                   float hp_left, float dist)
                {
                    bool headshot = IsHead(part);
                    int line = 0;
                    for (int m = 0; m < Settings::HITDATA_MODE_COUNT; ++m)
                    {
                        if (!g_Settings.hitdata.modes[m])
                            continue;

                        HitText t;
                        t.origin = origin;
                        t.origin.y += (float)line * 0.45f;
                        t.age = 0.0f;
                        t.life = g_Settings.hitdata.duration;
                        if (t.life < 0.35f)
                            t.life = 0.35f;
                        t.headshot = headshot;
                        FormatHitLine(t.text, sizeof(t.text), m, part, damage, hp_left, dist);
                        if (t.text[0] == '\0')
                            continue;
                        g_texts.push_back(t);
                        ++line;
                    }
                    while (g_texts.size() > 36)
                        g_texts.erase(g_texts.begin());
                }

                void DrawPixelBurst(ImDrawList* dl, const KillFx& fx, const Matrix4x4& vm,
                                    const Vector2& vp, float sx, float sy, float t) {
                    const float a = 1.0f - t;
                    const ImU32 acc = Accent();
                    const ImU32 soft = SoftAccent();
                    const ImU32 hi = TextCol();

                    ImVec2 c;
                    if (!Project(vm, vp, sx, sy, fx.origin, c)) return;
                    const float sr = ScreenRadius(vm, vp, sx, sy, fx.origin, 3.8f);

                    dl->AddCircleFilled(c, sr * 0.22f * (1.0f - t * 0.6f),
                                        MulA(acc, a * 0.35f), 28);
                    dl->AddCircleFilled(c, sr * 0.10f * (1.0f - t),
                                        MulA(hi, a * 0.75f), 20);

                    for (int i = 0; i < 48; ++i) {
                        const float u = Hash11(fx.seed + i * 1.9f);
                        const float v = Hash11(fx.seed + i * 2.7f);
                        const float w = Hash11(fx.seed + i * 3.3f);
                        const float ang = u * 6.2831853f;
                        const float elev = (v - 0.5f) * 1.6f;
                        const float dist = t * (2.5f + w * 7.5f);
                        const float rise = t * (1.0f + v * 3.5f) - t * t * 2.5f;
                        Vector3 wp{
                            fx.origin.x + std::cos(ang) * dist,
                            fx.origin.y + rise + elev * dist * 0.25f,
                            fx.origin.z + std::sin(ang) * dist
                        };
                        ImVec2 sp;
                        if (!Project(vm, vp, sx, sy, wp, sp)) continue;

                        const float cell = (std::max)(2.5f, sr * 0.09f * (1.0f - t * 0.35f));
                        const float pulse = 0.55f + 0.45f * std::sin(t * 18.0f + u * 6.0f);
                        ImU32 col = (i % 5 == 0) ? hi : ((i % 2) ? acc : soft);
                        col = MulA(col, a * pulse);

                        dl->AddRectFilled(ImVec2(sp.x + 1.0f, sp.y + 1.0f),
                                          ImVec2(sp.x + cell + 1.0f, sp.y + cell + 1.0f),
                                          MulA(IM_COL32(0, 0, 0, 255), a * 0.35f));
                        dl->AddRectFilled(ImVec2(sp.x, sp.y),
                                          ImVec2(sp.x + cell, sp.y + cell), col);
                        dl->AddRect(ImVec2(sp.x, sp.y),
                                    ImVec2(sp.x + cell, sp.y + cell),
                                    MulA(hi, a * 0.35f), 0.0f, 0, 1.0f);
                    }

                    for (int r = 0; r < 2; ++r) {
                        float u = t * 1.25f - r * 0.18f;
                        if (u < 0.f) u = 0.f;
                        if (u > 1.f) u = 1.f;
                        dl->AddCircle(c, sr * (0.35f + u * 2.1f),
                                      MulA(acc, a * (1.0f - u) * 0.85f), 48, 1.5f + (1.0f - u));
                    }
                }

                void DrawConfetti(ImDrawList* dl, const KillFx& fx, const Matrix4x4& vm,
                                  const Vector2& vp, float sx, float sy, float t) {
                    const float a = 1.0f - t;
                    const ImU32 palette[6] = {
                        Accent(), SoftAccent(), TextCol(),
                        LerpU32(Accent(), IM_COL32(255, 80, 160, 255), 0.45f),
                        LerpU32(Accent(), IM_COL32(80, 255, 200, 255), 0.40f),
                        LerpU32(Accent(), IM_COL32(255, 220, 80, 255), 0.50f),
                    };

                    for (int i = 0; i < 36; ++i) {
                        const float u = Hash11(fx.seed + i * 2.3f);
                        const float v = Hash11(fx.seed + i * 4.7f);
                        const float w = Hash11(fx.seed + i * 6.1f);
                        const float ang = u * 6.2831853f;
                        const float spd = 4.0f + v * 11.0f;
                        const float dist = t * spd;
                        const float grav = t * t * 14.0f;
                        Vector3 wp{
                            fx.origin.x + std::cos(ang) * dist,
                            fx.origin.y + t * (5.5f + w * 4.0f) - grav,
                            fx.origin.z + std::sin(ang) * dist
                        };
                        ImVec2 sp;
                        if (!Project(vm, vp, sx, sy, wp, sp)) continue;

                        const float rot = (u + t * (2.5f + v * 3.0f)) * 6.2831853f;
                        const float len = 4.5f + v * 4.0f;
                        const float thick = 1.6f + w * 1.4f;
                        const ImVec2 d(std::cos(rot) * len, std::sin(rot) * len);
                        const ImVec2 n(-std::sin(rot) * thick, std::cos(rot) * thick);

                        const ImU32 col = MulA(palette[i % 6], a * (0.75f + 0.25f * (1.0f - t)));

                        const ImVec2 p0(sp.x - d.x - n.x, sp.y - d.y - n.y);
                        const ImVec2 p1(sp.x + d.x - n.x, sp.y + d.y - n.y);
                        const ImVec2 p2(sp.x + d.x + n.x, sp.y + d.y + n.y);
                        const ImVec2 p3(sp.x - d.x + n.x, sp.y - d.y + n.y);
                        dl->AddQuadFilled(p0, p1, p2, p3, col);
                        dl->AddQuad(p0, p1, p2, p3, MulA(IM_COL32(255, 255, 255, 255), a * 0.25f), 1.0f);

                        if ((i & 1) == 0)
                            dl->AddCircleFilled(sp, 1.4f + (1.0f - t), MulA(TextCol(), a * 0.7f), 8);
                    }

                    ImVec2 c;
                    if (Project(vm, vp, sx, sy, fx.origin, c)) {
                        const float sr = ScreenRadius(vm, vp, sx, sy, fx.origin, 2.5f);
                        dl->AddCircleFilled(c, sr * 0.15f * (1.0f - t), MulA(Accent(), a * 0.4f), 20);
                    }
                }

                void DrawShatter(ImDrawList* dl, const KillFx& fx, const Matrix4x4& vm,
                                 const Vector2& vp, float sx, float sy, float t) {
                    const float a = 1.0f - t;
                    const ImU32 acc = Accent();
                    const ImU32 glass = SoftAccent();
                    const ImU32 edge = TextCol();

                    ImVec2 c;
                    if (!Project(vm, vp, sx, sy, fx.origin, c)) return;
                    const float sr = ScreenRadius(vm, vp, sx, sy, fx.origin, 3.6f);

                    for (int r = 0; r < 3; ++r) {
                        float u = t * 1.5f - r * 0.12f;
                        if (u < 0.f) u = 0.f;
                        if (u > 1.f) u = 1.f;
                        dl->AddCircle(c, sr * (0.2f + u * 2.4f),
                                      MulA(acc, a * (1.0f - u) * 0.7f), 40, 1.25f);
                    }

                    if (t < 0.2f) {
                        const float flash = 1.0f - t / 0.2f;
                        dl->AddCircleFilled(c, sr * 0.35f * flash, MulA(edge, flash * 0.55f), 24);
                    }

                    for (int i = 0; i < 22; ++i) {
                        const float u = Hash11(fx.seed + i);
                        const float v = Hash11(fx.seed + i + 40.f);
                        const float w = Hash11(fx.seed + i + 80.f);
                        const float ang = u * 6.2831853f;
                        const float elev = (v - 0.35f) * 1.4f;
                        const float dist = t * (2.0f + w * 6.5f);
                        const float fall = t * t * 5.0f;
                        Vector3 mid{
                            fx.origin.x + std::cos(ang) * dist,
                            fx.origin.y + elev * dist - fall,
                            fx.origin.z + std::sin(ang) * dist
                        };

                        ImVec2 sp;
                        if (!Project(vm, vp, sx, sy, mid, sp)) continue;

                        const float spin = t * (4.0f + u * 6.0f) + ang;
                        const float s = sr * (0.18f + v * 0.14f) * (1.0f - t * 0.35f);
                        const ImVec2 p0(sp.x + std::cos(spin) * s,
                                        sp.y + std::sin(spin) * s);
                        const ImVec2 p1(sp.x + std::cos(spin + 2.15f) * s * 0.75f,
                                        sp.y + std::sin(spin + 2.15f) * s * 0.75f);
                        const ImVec2 p2(sp.x + std::cos(spin - 2.05f) * s * 0.70f,
                                        sp.y + std::sin(spin - 2.05f) * s * 0.70f);

                        dl->AddTriangleFilled(p0, p1, p2, MulA(glass, a * 0.42f));
                        dl->AddTriangle(p0, p1, p2, MulA(edge, a * 0.85f), 1.2f);

                        dl->AddLine(p0, p1, MulA(acc, a * 0.65f), 1.0f);
                    }
                }

                void DrawKillFx(ImDrawList* dl, const KillFx& fx, const Matrix4x4& vm,
                                const Vector2& vp, float sx, float sy)
                {
                    float t = fx.age / fx.life;
                    if (t < 0.f) t = 0.f;
                    if (t > 1.f) t = 1.f;

                    if (fx.type == Confetti)
                        DrawConfetti(dl, fx, vm, vp, sx, sy, t);

                    else if (fx.type == Shatter)
                        DrawShatter(dl, fx, vm, vp, sx, sy, t);

                    else
                        DrawPixelBurst(dl, fx, vm, vp, sx, sy, t);
                }

                void DrawHitmarker(ImDrawList* dl, const Marker& m, const Matrix4x4& vm,
                                   const Vector2& vp, float sx, float sy)
                {
                    float t = m.age / m.life;
                    if (t < 0.f) t = 0.f;
                    if (t > 1.f) t = 1.f;

                    float fade_in = t / 0.08f;
                    if (fade_in < 0.f) fade_in = 0.f;
                    if (fade_in > 1.f) fade_in = 1.f;

                    float fade_out = 1.0f - t;
                    float a = fade_in * fade_out;

                    ImVec2 c;
                    if (!Project(vm, vp, sx, sy, m.origin, c)) return;
                    const float base = ScreenRadius(vm, vp, sx, sy, m.origin, 0.55f)
                        * (std::max)(0.55f, g_Settings.hitmarker.size);
                    const float pop = 1.0f + (1.0f - fade_in) * 0.55f;
                    const float s = base * pop;

                    const ImU32 acc = m.headshot ? SoftAccent() : Accent();
                    const ImU32 hi = TextCol();
                    const ImU32 shadow = IM_COL32(0, 0, 0, 180);

                    dl->AddCircle(c, s * (1.1f + t * 1.8f), MulA(acc, a * 0.55f), 36, 1.5f);

                    const ImVec2 d0(c.x, c.y - s * 0.55f);
                    const ImVec2 d1(c.x + s * 0.55f, c.y);
                    const ImVec2 d2(c.x, c.y + s * 0.55f);
                    const ImVec2 d3(c.x - s * 0.55f, c.y);
                    dl->AddQuad(ImVec2(d0.x + 1, d0.y + 1), ImVec2(d1.x + 1, d1.y + 1),
                                ImVec2(d2.x + 1, d2.y + 1), ImVec2(d3.x + 1, d3.y + 1),
                                MulA(shadow, a), 1.5f);
                    dl->AddQuad(d0, d1, d2, d3, MulA(acc, a), 1.6f);

                    const float gap = s * 0.22f;
                    const float arm = s * 0.85f;
                    const float th = 2.0f;
                    auto arm_line = [&](ImVec2 a0, ImVec2 a1) {
                        dl->AddLine(ImVec2(a0.x + 1, a0.y + 1), ImVec2(a1.x + 1, a1.y + 1),
                                    MulA(shadow, a), th + 1.0f);
                        dl->AddLine(a0, a1, MulA(hi, a), th);
                        dl->AddLine(a0, a1, MulA(acc, a * 0.85f), th * 0.55f);
                    };
                    arm_line(ImVec2(c.x - arm, c.y - arm), ImVec2(c.x - gap, c.y - gap));
                    arm_line(ImVec2(c.x + gap, c.y - gap), ImVec2(c.x + arm, c.y - arm));
                    arm_line(ImVec2(c.x - arm, c.y + arm), ImVec2(c.x - gap, c.y + gap));
                    arm_line(ImVec2(c.x + gap, c.y + gap), ImVec2(c.x + arm, c.y + arm));

                    dl->AddCircleFilled(c, s * 0.12f * (1.0f - t * 0.5f), MulA(hi, a * 0.9f), 12);
                }

                void DrawHitText(ImDrawList* dl, const HitText& ht, const Matrix4x4& vm,
                                 const Vector2& vp, float sx, float sy)
                {
                    float t = ht.age / ht.life;
                    if (t < 0.f) t = 0.f;
                    if (t > 1.f) t = 1.f;

                    float fade_in = t / 0.08f;
                    if (fade_in < 0.f) fade_in = 0.f;
                    if (fade_in > 1.f) fade_in = 1.f;

                    float fo = (t - 0.6f) / 0.4f;
                    if (fo < 0.f) fo = 0.f;
                    if (fo > 1.f) fo = 1.f;
                    float fade_out = 1.0f - fo;

                    float a = fade_in * fade_out;
                    if (a <= 0.01f)
                        return;

                    Vector3 wp = ht.origin;
                    wp.y += t * 2.2f;

                    ImVec2 sp;
                    if (!Project(vm, vp, sx, sy, wp, sp)) return;

                    ImFont* font = GuiFont();
                    const float fs = fonts::snap_px((std::max)(10.0f, g_Settings.hitdata.size));
                    const ImVec2 tsz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, ht.text);
                    const ImVec2 pos(sp.x - tsz.x * 0.5f, sp.y - tsz.y * 0.5f);

                    const ImU32 col = ht.headshot
                        ? SoftAccent(a)
                        : colors::text_active_u32(a);
                    dl->AddText(font, fs, pos, col, ht.text);
                }

            }

            const char* const* EffectNames() { return k_fx_names; }
            const char* const* HitDataModeNames() { return k_hitdata_names; }

            void Tick()
            {
                bool any = g_Settings.killfx.enabled
                    || g_Settings.hitmarker.enabled
                    || g_Settings.hitdata.enabled
                    || g_Settings.hitsound.enabled
                    || g_Settings.esp.hit_chams;

                if (!any)
                {
                    g_pending.clear();
                    g_fx.clear();
                    g_markers.clear();
                    g_texts.clear();
                    hit_chams_state::list.clear();
                    g_watch = {};
                    g_lmb_was = false;
                    g_lmb_at = -1.0;
                    return;
                }
                if (!Globals::Workspace || !Globals::InstanceDataModel.address)
                    return;

                double now = ImGui::GetTime();
                float dt = ImGui::GetIO().DeltaTime;

                std::uint64_t target = Features::Aim::CurrentTarget();
                bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
                if (lmb && !g_lmb_was)
                    g_lmb_at = now;
                g_lmb_was = lmb;

                bool recent_shot = lmb || (g_lmb_at >= 0.0 && (now - g_lmb_at) < 0.45);

                int aim_part = Features::Aim::CurrentAimPart();
                Vector3 aim_point = Features::Aim::CurrentAimPoint();

                if (g_Settings.hitmarker.enabled || g_Settings.hitdata.enabled
                    || g_Settings.hitsound.enabled || g_Settings.esp.hit_chams)
                {
                    if (target)
                    {
                        if (g_watch.addr != target)
                        {
                            g_watch.addr = target;
                            g_watch.last_hp = -1.0f;
                            g_watch.last_fx = 0.0;
                        }
                        g_watch.part = aim_part;
                        g_watch.point = aim_point;
                        g_watch.keep_until = now + 0.85;
                    }

                    if (g_watch.addr && now <= g_watch.keep_until)
                    {
                        Vector3 pos = g_watch.point;
                        float hp = 0.0f;
                        bool found = false;
                        LookupTarget(g_watch.addr, pos, hp, found);

                        if (found)
                        {
                            if (g_watch.last_hp < 0.0f)
                            {
                                g_watch.last_hp = hp;
                            }

                            else
                            {
                                float drop = g_watch.last_hp - hp;
                                if (drop > 0.25f && recent_shot && (now - g_watch.last_fx) > 0.07)
                                {
                                    int part = g_watch.part;
                                    Vector3 hit_pos = g_watch.point;
                                    if (hit_pos.x == 0.f && hit_pos.y == 0.f && hit_pos.z == 0.f)
                                        hit_pos = pos;
                                    if (IsHead(part))
                                        hit_pos.y += 0.2f;

                                    else
                                        hit_pos.y += 0.08f;

                                    float dist = 0.0f;
                                    if (auto cam = Globals::Workspace->GetCurrentCamera())
                                        dist = (hit_pos - Camera(cam->address).GetPosition()).Length();

                                    if (g_Settings.hitmarker.enabled)
                                        SpawnMarker(hit_pos, IsHead(part));
                                    if (g_Settings.hitdata.enabled)
                                        SpawnHitTexts(hit_pos, part, drop, hp, dist);
                                    if (g_Settings.hitsound.enabled)
                                        Features::HitSounds::PlayCurrent();
                                    if (g_Settings.esp.hit_chams)
                                        NotifyHitChams(g_watch.addr);

                                    g_watch.last_fx = now;
                                }

                                g_watch.last_hp = hp;
                            }
                        }

                        else if (now > g_watch.keep_until)
                        {
                            g_watch = {};
                        }
                    }

                    else if (now > g_watch.keep_until)
                    {
                        g_watch = {};
                    }
                }

                else
                {
                    g_watch = {};
                }

                if (g_Settings.killfx.enabled && target && lmb)
                {
                    Vector3 pos{};
                    float hp = 100.0f;
                    bool found = false;
                    LookupTarget(target, pos, hp, found);
                    if (found)
                    {
                        bool have = false;
                        for (auto& p : g_pending)
                        {
                            if (p.addr == target)
                            {
                                p.click_at = now;
                                if (hp > 0.0f) p.was_alive = true;
                                if (pos.x != 0 || pos.y != 0 || pos.z != 0) p.pos = pos;
                                have = true;
                                break;
                            }
                        }
                        if (!have && hp > 0.0f)
                        {
                            PendingKill p;
                            p.addr = target;
                            p.pos = pos;
                            p.click_at = now;
                            p.was_alive = true;
                            g_pending.push_back(p);
                        }
                    }
                }

                if (g_Settings.killfx.enabled)
                {
                    for (size_t i = 0; i < g_pending.size(); )
                    {
                        auto& p = g_pending[i];
                        if (now - p.click_at > 2.0)
                        {
                            g_pending.erase(g_pending.begin() + (std::ptrdiff_t)i);
                            continue;
                        }

                        Vector3 pos = p.pos;
                        float hp = 0.0f;
                        bool found = false;
                        bool has_pos = LookupTarget(p.addr, pos, hp, found);
                        if (has_pos) p.pos = pos;

                        bool dead = !found || hp <= 0.0f;
                        if (p.was_alive && dead)
                        {
                            SpawnKill(g_Settings.killfx.effect, p.pos);
                            g_pending.erase(g_pending.begin() + (std::ptrdiff_t)i);
                            continue;
                        }
                        if (found && hp > 0.0f) p.was_alive = true;
                        ++i;
                    }
                }

                else
                {
                    g_pending.clear();
                }

                for (auto& fx : g_fx) fx.age += dt;
                for (auto& m : g_markers) m.age += dt;
                for (auto& t : g_texts) t.age += dt;
                for (auto& h : hit_chams_state::list) h.age += dt;

                g_fx.erase(std::remove_if(g_fx.begin(), g_fx.end(),
                    [](const KillFx& f) { return f.age >= f.life; }), g_fx.end());
                g_markers.erase(std::remove_if(g_markers.begin(), g_markers.end(),
                    [](const Marker& m) { return m.age >= m.life; }), g_markers.end());
                g_texts.erase(std::remove_if(g_texts.begin(), g_texts.end(),
                    [](const HitText& t) { return t.age >= t.life; }), g_texts.end());
                hit_chams_state::list.erase(std::remove_if(
                    hit_chams_state::list.begin(), hit_chams_state::list.end(),
                    [](const hit_chams_state::HitChamFx& h) { return h.age >= h.life; }),
                    hit_chams_state::list.end());

                if (g_fx.empty() && g_markers.empty() && g_texts.empty() &&
                    hit_chams_state::list.empty())
                    return;

                auto cam_ptr = Globals::Workspace->GetCurrentCamera();
                if (!cam_ptr)
                    return;
                Camera camera(cam_ptr->address);
                Vector2 viewport = camera.GetViewportSize();

                float sx = 1.0f, sy = 1.0f;
                HWND oh = Renderer::GetHwnd();
                if (oh)
                {
                    RECT ocr{};
                    if (GetClientRect(oh, &ocr) && viewport.x > 1.f && viewport.y > 1.f)
                    {
                        sx = (float)(ocr.right - ocr.left) / viewport.x;
                        sy = (float)(ocr.bottom - ocr.top) / viewport.y;
                    }
                }

                static uintptr_t s_base = 0;
                if (!s_base) s_base = g_Memory.GetModuleBase();
                uintptr_t ve = g_Memory.Read<uintptr_t>(s_base + ::VisualEngine::Pointer);
                Matrix4x4 vm = g_Memory.Read<Matrix4x4>(ve + ::VisualEngine::ViewMatrix);

                ImDrawList* dl = ImGui::GetBackgroundDrawList();
                if (!dl)
                    return;

                for (const auto& fx : g_fx)
                    DrawKillFx(dl, fx, vm, viewport, sx, sy);
                for (const auto& m : g_markers)
                    DrawHitmarker(dl, m, vm, viewport, sx, sy);
                for (const auto& t : g_texts)
                    DrawHitText(dl, t, vm, viewport, sx, sy);
            }

        }
    }
}
