#include "pch.h"
#define NOMINMAX
#include "Aim.h"
#include "RaycastSilent.h"
#include "MagicBullet.h"
#include "ViewportSilent.h"
#include "MouseSilent.h"
#include "PhantomSilent.h"
#include "app/Settings.h"
#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/math/Math.h"
#include "core/roblox/classes/Classes.h"
#include "core/player/PlayerHandler.h"
#include "features/visuals/RaycastEngine.h"
#include "features/visuals/HavocWorldEsp.h"
#include "features/games/PhantomForces.h"
#include "renderer/Renderer.h"
#include "imgui.h"
#include <Windows.h>
#include <cmath>
#include <cfloat>
#include <algorithm>
#include <cstdint>

namespace Cheat {
    namespace Features {

        namespace {
            using Config = Settings::AimbotConfig;

            bool s_was_pressed = false;
            bool s_toggled = false;
            bool s_silent_was = false;
            bool s_silent_tog = false;
            bool s_force_mb_was = false;
            bool s_force_mb_tog = false;

            std::uint64_t s_target = 0;
            int s_cycle = 0;
            double s_last_switch = 0.0;
            double s_engage_at = 0.0;
            std::uint64_t s_pending = 0;
            int s_aim_part = Settings::AIM_HEAD;
            Vector3 s_aim_point{};
            std::uint32_t s_rng = 0xA341316Cu;

            const Instance* part_instance(const PlayerCache& c, int part)
            {
                if (part == Settings::AIM_HEAD)
                {
                    return c.head.get();
                }

                if (part == Settings::AIM_UPPER_TORSO)
                {
                    return c.upperTorso.get();
                }

                if (part == Settings::AIM_LOWER_TORSO)
                {
                    // R6: один Torso
                    if (c.lowerTorso)
                        return c.lowerTorso.get();
                    return c.upperTorso.get();
                }

                if (part == Settings::AIM_HRP)
                {
                    if (c.humanoidRootPart)
                        return c.humanoidRootPart.get();
                    return c.upperTorso.get();
                }

                if (part == Settings::AIM_LEFT_HAND)
                {
                    if (c.leftHand)
                        return c.leftHand.get();
                    return c.leftUpperArm.get(); // R6 Left Arm
                }

                if (part == Settings::AIM_RIGHT_HAND)
                {
                    if (c.rightHand)
                        return c.rightHand.get();
                    return c.rightUpperArm.get();
                }

                if (part == Settings::AIM_LEFT_FOOT)
                {
                    if (c.leftFoot)
                        return c.leftFoot.get();
                    return c.leftUpperLeg.get(); // R6 Left Leg
                }

                if (part == Settings::AIM_RIGHT_FOOT)
                {
                    if (c.rightFoot)
                        return c.rightFoot.get();
                    return c.rightUpperLeg.get();
                }

                return nullptr;
            }

            // xorshift, пойдёт
            std::uint32_t next_rng() {
                s_rng ^= s_rng << 13;
                s_rng ^= s_rng >> 17;
                s_rng ^= s_rng << 5;
                return s_rng;
            }

            int part_tier_of(const Config& cfg, int part)
            {
                if (part < 0 || part >= Settings::AIM_PART_COUNT)
                {
                    return Settings::PART_OFF;
                }

                int t = cfg.part_tier[part];
                // старый чекбокс parts[] ещё живёт
                if (t == Settings::PART_OFF && cfg.parts[part])
                {
                    t = Settings::PART_PRIMARY;
                }
                return t;
            }

            struct PartSample {
                int part = Settings::AIM_HEAD;
                int tier = Settings::PART_PRIMARY;
                Vector3 world{};
                Vector2 screen{};
                float dist = FLT_MAX;
            };

            int pick_among(const Config& cfg, PartSample* list, int n, bool allow_reroll)
            {
                if (n <= 0)
                {
                    return -1;
                }

                if (n == 1)
                {
                    return 0;
                }

                if (cfg.part_select == Settings::PART_SELECT_CLOSEST)
                {
                    int best = 0;
                    for (int i = 1; i < n; ++i)
                    {
                        if (list[i].dist < list[best].dist)
                        {
                            best = i;
                        }
                    }
                    return best;
                }

                double now = ImGui::GetTime();
                float sw = cfg.switch_time;
                if (sw < 0.05f) sw = 0.05f;

                if (allow_reroll && now - s_last_switch >= sw)
                {
                    if (cfg.part_select == Settings::PART_SELECT_RANDOM)
                    {
                        s_cycle = (int)(next_rng() % (std::uint32_t)n);
                    }

                    else
                    {
                        s_cycle = (s_cycle + 1) % n;
                    }
                    s_last_switch = now;
                }

                if (s_cycle < 0 || s_cycle >= n)
                {
                    s_cycle = 0;
                }
                return s_cycle;
            }

            bool hitchance_ok(const Config& cfg)
            {
                if (!cfg.hitchance_enabled)
                {
                    return true;
                }

                float chance = cfg.hitchance;
                if (chance < 1.f) chance = 1.f;
                if (chance > 100.f) chance = 100.f;

                // 0..100, хватит
                float roll = (float)(next_rng() % 10000u) / 100.f;
                return roll <= chance;
            }

            ImVec2 overlay_size()
            {
                HWND oh = Renderer::GetHwnd();
                if (oh)
                {
                    RECT ocr{};
                    if (GetClientRect(oh, &ocr))
                    {
                        float w = (float)(ocr.right - ocr.left);
                        float h = (float)(ocr.bottom - ocr.top);
                        if (w < 1.f) w = 1.f;
                        if (h < 1.f) h = 1.f;
                        return ImVec2(w, h);
                    }
                }
                return ImGui::GetIO().DisplaySize;
            }

            bool game_client_screen_rect(RECT& out)
            {
                HWND hwnd = Renderer::GetGameHwnd();
                if (!hwnd || !IsWindow(hwnd))
                {
                    hwnd = Renderer::GetHwnd();
                }

                if (!hwnd || !IsWindow(hwnd))
                {
                    return false;
                }

                RECT cr{};
                if (!GetClientRect(hwnd, &cr))
                {
                    return false;
                }

                POINT tl{ cr.left, cr.top };
                POINT br{ cr.right, cr.bottom };
                ClientToScreen(hwnd, &tl);
                ClientToScreen(hwnd, &br);
                if (br.x - tl.x < 8 || br.y - tl.y < 8)
                {
                    return false;
                }

                out.left = tl.x;
                out.top = tl.y;
                out.right = br.x;
                out.bottom = br.y;
                return true;
            }

            bool s_cursor_clipped = false;

            void release_cursor_clip()
            {
                if (!s_cursor_clipped)
                {
                    return;
                }
                ClipCursor(nullptr);
                s_cursor_clipped = false;
            }

            void clear_silents()
            {
                ViewportSilent::SetActive(false);
                MouseSilent::SetActive(false);
                PhantomSilent::SetActive(false);
            }

            void lock_cursor_to_game()
            {
                RECT r{};
                if (!game_client_screen_rect(r))
                {
                    release_cursor_clip();
                    return;
                }
                ClipCursor(&r);
                s_cursor_clipped = true;

                POINT p{};
                if (!GetCursorPos(&p))
                {
                    return;
                }

                LONG x = p.x;
                LONG y = p.y;
                if (x < r.left) x = r.left;
                if (x > r.right - 1) x = r.right - 1;
                if (y < r.top) y = r.top;
                if (y > r.bottom - 1) y = r.bottom - 1;

                if (x != p.x || y != p.y)
                {
                    SetCursorPos(x, y);
                }
            }

            ImVec2 cursor_client()
            {
                POINT p{};
                ImVec2 sz = overlay_size();
                if (!GetCursorPos(&p))
                {
                    return ImVec2(sz.x * 0.5f, sz.y * 0.5f);
                }

                HWND overlay = Renderer::GetHwnd();
                if (overlay)
                {
                    ScreenToClient(overlay, &p);
                }

                float x = (float)p.x;
                float y = (float)p.y;
                // вне оверлея в центр, похуй
                if (x < 0.f || y < 0.f || x > sz.x || y > sz.y)
                {
                    return ImVec2(sz.x * 0.5f, sz.y * 0.5f);
                }

                return ImVec2(x, y);
            }

            ImVec2 fov_anchor(const Config& cfg)
            {
                ImVec2 sz = overlay_size();
                if (cfg.fov_position == 1)
                {
                    return cursor_client();
                }

                return ImVec2(sz.x * 0.5f, sz.y * 0.5f);
            }

            void draw_fov(const Config& cfg)
            {
                if (!cfg.fov_enabled)
                {
                    return;
                }

                ImDrawList* dl = ImGui::GetBackgroundDrawList();
                if (!dl)
                {
                    return;
                }

                ImVec2 center = fov_anchor(cfg);
                float radius = cfg.fov_size;
                if (radius < 1.f) radius = 1.f;

                ImU32 outline = ImGui::ColorConvertFloat4ToU32(ImVec4(
                    cfg.fov_outline_color[0], cfg.fov_outline_color[1],
                    cfg.fov_outline_color[2], cfg.fov_outline_color[3]));

                if (cfg.fov_style == 1)
                {
                    float fill_a = cfg.fov_color[3] * 0.35f;
                    float rim_a = cfg.fov_color[3];
                    if (rim_a > 1.f) rim_a = 1.f;

                    ImU32 fill = ImGui::ColorConvertFloat4ToU32(ImVec4(
                        cfg.fov_color[0], cfg.fov_color[1], cfg.fov_color[2], fill_a));
                    ImU32 rim = ImGui::ColorConvertFloat4ToU32(ImVec4(
                        cfg.fov_color[0], cfg.fov_color[1], cfg.fov_color[2], rim_a));
                    dl->AddCircleFilled(center, radius, fill, 64);
                    dl->AddCircle(center, radius, rim, 64, 1.75f);
                }

                else
                {
                    ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(
                        cfg.fov_color[0], cfg.fov_color[1], cfg.fov_color[2], cfg.fov_color[3]));
                    dl->AddCircle(center, radius, col, 64, 1.5f);
                }

                if (cfg.fov_outline)
                {
                    dl->AddCircle(center, radius + 1.0f, outline, 64, 2.0f);
                    dl->AddCircle(center, radius - 1.0f,
                        IM_COL32(0, 0, 0, (int)(cfg.fov_outline_color[3] * 100.0f)), 64, 1.0f);
                }
            }

            void draw_aim_tracer(const Config& cfg, const Vector2& target_screen)
            {
                if (!cfg.tracer)
                {
                    return;
                }

                ImDrawList* dl = ImGui::GetForegroundDrawList();
                if (!dl)
                {
                    return;
                }

                ImVec2 from = fov_anchor(cfg);
                ImVec2 to(target_screen.x, target_screen.y);
                ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(
                    cfg.tracer_color[0], cfg.tracer_color[1],
                    cfg.tracer_color[2], cfg.tracer_color[3]));

                dl->AddLine(from, to, IM_COL32(0, 0, 0, (int)(cfg.tracer_color[3] * 180.f)), 2.5f);
                dl->AddLine(from, to, col, 1.25f);
            }

            bool world_to_screen(const Matrix4x4& m, const Vector2& dim,
                                 const Vector3& p, Vector2& out)
            {
                float w = p.x * m.m[3][0] + p.y * m.m[3][1] + p.z * m.m[3][2] + m.m[3][3];
                if (w < 0.01f)
                {
                    return false;
                }

                float x = p.x * m.m[0][0] + p.y * m.m[0][1] + p.z * m.m[0][2] + m.m[0][3];
                float y = p.x * m.m[1][0] + p.y * m.m[1][1] + p.z * m.m[1][2] + m.m[1][3];
                float inv = 1.0f / w;
                out.x = (dim.x * 0.5f) + (x * inv * dim.x * 0.5f);
                out.y = (dim.y * 0.5f) - (y * inv * dim.y * 0.5f);

                // оверлей может быть не того размера что viewport камеры
                HWND oh = Renderer::GetHwnd();
                if (oh)
                {
                    RECT ocr{};
                    if (GetClientRect(oh, &ocr) && dim.x > 1.0f && dim.y > 1.0f)
                    {
                        out.x *= (float)(ocr.right - ocr.left) / dim.x;
                        out.y *= (float)(ocr.bottom - ocr.top) / dim.y;
                    }
                }
                return true;
            }

            struct Scene {
                bool ok = false;
                Matrix4x4 view{};
                Vector2 viewport{};
                Camera camera{ 0 };
                Vector3 cam_pos{};
                Vector3 local_pos{}; // hrp, для дистанции
                std::uint64_t local_player = 0;
                std::uint64_t local_char = 0;
                std::uint64_t local_team_folder = 0;
            };

            bool is_local_entry(const PlayerCache& cache, const Scene& sc)
            {
                if (sc.local_player && cache.address == sc.local_player)
                {
                    return true;
                }

                if (sc.local_char && cache.address == sc.local_char)
                {
                    return true;
                }

                return false;
            }

            bool is_teammate(const PlayerCache& cache, const Scene& sc)
            {
                if (!g_Settings.misc.teamcheck)
                {
                    return false;
                }

                return PlayerHandler::IsTeammate(cache, sc.local_team_folder);
            }

            bool is_dead_player(const PlayerCache& cache)
            {
                if (cache.is_corpse)
                {
                    return true;
                }

                if (!cache.humanoid || !g_Memory.IsValid(cache.humanoid->address))
                {
                    return false;
                }

                Humanoid hum(cache.humanoid->address);
                if (hum.GetStateId() == 15)
                {
                    return true;
                }

                if (hum.GetHealth() <= 0.0f)
                {
                    return true;
                }

                return false;
            }

            Scene resolve_scene()
            {
                Scene s;
                if (!Globals::Workspace || !Globals::Players)
                {
                    return s;
                }

                auto cam = Globals::Workspace->GetCurrentCamera();
                if (!cam)
                {
                    return s;
                }

                s.camera = Camera(cam->address);
                s.viewport = s.camera.GetViewportSize();
                s.cam_pos = s.camera.GetPosition();
                s.local_pos = s.cam_pos;

                static uintptr_t base = g_Memory.GetModuleBase();
                if (!base)
                {
                    base = g_Memory.GetModuleBase();
                }
                uintptr_t ve = g_Memory.Read<uintptr_t>(base + ::VisualEngine::Pointer);
                s.view = g_Memory.Read<Matrix4x4>(ve + ::VisualEngine::ViewMatrix);

                if (g_Memory.IsValid(Globals::Players->address))
                {
                    s.local_player = g_Memory.Read<std::uint64_t>(
                        Globals::Players->address + ::Player::LocalPlayer);
                    if (g_Memory.IsValid(s.local_player))
                    {
                        s.local_char = g_Memory.Read<std::uint64_t>(
                            s.local_player + ::Player::ModelInstance);
                        PlayerCache loc = PlayerHandler::GetCachedPlayer(s.local_player);
                        if (loc.humanoidRootPart && g_Memory.IsValid(loc.humanoidRootPart->address))
                        {
                            s.local_pos = BasePart(loc.humanoidRootPart->address).GetPosition();
                        }

                        if (g_Settings.misc.teamcheck)
                        {
                            s.local_team_folder = PlayerHandler::LocalTeamFolder();
                        }
                    }
                }

                s.ok = true;
                return s;
            }

            bool collect_parts(const PlayerCache& cache, const Config& cfg, const Scene& sc,
                               const Vector2& anchor_v, PartSample* out, int& out_n)
            {
                out_n = 0;
                bool has_parts = false;
                for (int i = 0; i < Settings::AIM_PART_COUNT; ++i)
                {
                    if (part_tier_of(cfg, i) != Settings::PART_OFF)
                    {
                        has_parts = true;
                        break;
                    }
                }

                for (int part = 0; part < Settings::AIM_PART_COUNT; ++part)
                {
                    int tier = part_tier_of(cfg, part);
                    // ничего не выбрано, дефолт голова
                    if (!has_parts)
                    {
                        if (part == Settings::AIM_HEAD)
                        {
                            tier = Settings::PART_PRIMARY;
                        }

                        else
                        {
                            tier = Settings::PART_OFF;
                        }
                    }

                    if (tier == Settings::PART_OFF)
                    {
                        continue;
                    }

                    const Instance* p = part_instance(cache, part);
                    if (!p || !g_Memory.IsValid(p->address))
                    {
                        continue;
                    }

                    BasePart bp(p->address);
                    Vector3 world = bp.GetPosition();
                    if (cfg.prediction)
                    {
                        Vector3 vel = bp.GetAssemblyLinearVelocity();
                        float dist = (world - sc.cam_pos).Length();
                        float lead = 0.f;
                        if (cfg.bullet_speed > 1.f)
                        {
                            lead = dist / cfg.bullet_speed;
                        }

                        else
                        {
                            lead = 0.05f; // скорость не задана, чуть-чуть
                        }
                        world.x += vel.x * lead;
                        world.y += vel.y * lead;
                        world.z += vel.z * lead;
                    }

                    {
                        float dist = (world - sc.local_pos).Length();
                        // havoc: кап 400m всегда
                        if (Visuals::HavocWorldEsp::BeyondRange(dist))
                        {
                            continue;
                        }

                        if (cfg.distance_check &&
                            dist > cfg.max_distance)
                        {
                            continue;
                        }
                    }

                    Vector2 screen{};
                    if (!world_to_screen(sc.view, sc.viewport, world, screen))
                    {
                        continue;
                    }

                    float dx = screen.x - anchor_v.x;
                    float dy = screen.y - anchor_v.y;
                    PartSample& s = out[out_n++];
                    s.part = part;
                    s.tier = tier;
                    s.world = world;
                    s.screen = screen;
                    s.dist = std::sqrt(dx * dx + dy * dy);
                }

                return out_n > 0;
            }

            bool choose_part(const Config& cfg, PartSample* samples, int n,
                             bool allow_reroll, PartSample& out)
            {
                if (n <= 0)
                {
                    return false;
                }

                int best_tier = 99;
                for (int i = 0; i < n; ++i)
                {
                    if (samples[i].tier > 0 && samples[i].tier < best_tier)
                    {
                        best_tier = samples[i].tier;
                    }
                }

                PartSample tiered[Settings::AIM_PART_COUNT];
                int tn = 0;
                for (int i = 0; i < n; ++i)
                {
                    if (samples[i].tier == best_tier)
                    {
                        tiered[tn++] = samples[i];
                    }
                }

                int idx = pick_among(cfg, tiered, tn, allow_reroll);
                if (idx < 0)
                {
                    return false;
                }

                out = tiered[idx];
                return true;
            }

            bool select_target(const Config& cfg, const Scene& sc,
                               Vector3& out_world, Vector2& out_screen)
            {
                ImVec2 anchor = fov_anchor(cfg);
                Vector2 anchor_v(anchor.x, anchor.y);

                float fov_r = 1e9f;
                if (cfg.fov_enabled)
                {
                    fov_r = cfg.fov_size;
                    if (fov_r < 1.f) fov_r = 1.f;
                }

                float sticky_scale = cfg.sticky_fov_scale;
                if (sticky_scale < 1.f) sticky_scale = 1.f;
                float sticky_r = fov_r * sticky_scale;

                bool found_sticky = false;
                PartSample sticky_pick{};
                bool found_best = false;
                float best_score = FLT_MAX;
                float best_tie = FLT_MAX;
                std::uint64_t best_addr = 0;
                PartSample best_pick{};

                // локала не целимся, даже если залип
                if (s_target && (s_target == sc.local_player || s_target == sc.local_char))
                {
                    s_target = 0;
                }

                if (s_pending && (s_pending == sc.local_player || s_pending == sc.local_char))
                {
                    s_pending = 0;
                }

                PlayerHandler::ForEachPlayer([&](const PlayerCache& cache)
                {
                    if (is_local_entry(cache, sc))
                    {
                        return;
                    }

                    if (is_teammate(cache, sc))
                    {
                        return;
                    }

                    if (!cache.is_player && !cache.is_corpse && !g_Settings.aim.target_bots)
                    {
                        return;
                    }

                    if (cfg.dead_check && is_dead_player(cache))
                    {
                        return;
                    }

                    if (cfg.visible_only && RaycastEngine::IsEnabled())
                    {
                        auto vis = RaycastEngine::IsPlayerVisible(cache, sc.cam_pos);
                        if (!vis.visible)
                        {
                            return;
                        }
                    }

                    PartSample samples[Settings::AIM_PART_COUNT];
                    int sn = 0;
                    if (!collect_parts(cache, cfg, sc, anchor_v, samples, sn))
                    {
                        return;
                    }

                    PartSample pick{};
                    if (!choose_part(cfg, samples, sn, cache.address == s_target, pick))
                    {
                        return;
                    }

                    if (cfg.sticky && s_target != 0 && cache.address == s_target &&
                        pick.dist <= sticky_r)
                    {
                        found_sticky = true;
                        sticky_pick = pick;
                    }

                    if (pick.dist > fov_r)
                    {
                        return;
                    }

                    float score = pick.dist;
                    float tie = pick.dist;
                    if (cfg.target_select == Settings::TARGET_DISTANCE)
                    {
                        score = (pick.world - sc.local_pos).Length();
                    }

                    else if (cfg.target_select == Settings::TARGET_LOWEST_HP)
                    {
                        score = FLT_MAX;
                        if (cache.humanoid && g_Memory.IsValid(cache.humanoid->address))
                        {
                            Humanoid hum(cache.humanoid->address);
                            float hp = hum.GetHealth();
                            if (hp > 0.f)
                            {
                                score = hp;
                            }
                        }
                    }

                    if (score < best_score || (score == best_score && tie < best_tie))
                    {
                        best_score = score;
                        best_tie = tie;
                        best_addr = cache.address;
                        best_pick = pick;
                        found_best = true;
                    }
                });

                auto commit = [&](std::uint64_t addr, const PartSample& pick) -> bool
                {
                    s_target = addr;
                    s_pending = 0;
                    out_world = pick.world;
                    out_screen = pick.screen;
                    s_aim_part = pick.part;
                    s_aim_point = pick.world;
                    return true;
                };

                if (cfg.sticky && found_sticky)
                {
                    return commit(s_target, sticky_pick);
                }

                if (!found_best)
                {
                    s_pending = 0;
                    s_target = 0;
                    return false;
                }

                if (!cfg.sticky)
                {
                    return commit(best_addr, best_pick);
                }

                // humanize, ждём reaction_ms перед свапом
                double now = ImGui::GetTime();
                if (cfg.humanize && cfg.reaction_ms > 0.0f && best_addr != s_target)
                {
                    if (best_addr != s_pending)
                    {
                        s_pending = best_addr;
                        s_engage_at = now + cfg.reaction_ms / 1000.0;
                        return false;
                    }

                    if (now < s_engage_at)
                    {
                        return false;
                    }
                }

                return commit(best_addr, best_pick);
            }

            void apply_mouse(const Config& cfg, const Vector2& target_screen)
            {
                lock_cursor_to_game();

                ImVec2 cur = fov_anchor(cfg);

                float sx = cfg.smooth_x;
                float sy = cfg.smooth_y;
                if (sx < 0.1f) sx = 0.1f;
                if (sy < 0.1f) sy = 0.1f;

                float dx = (target_screen.x - cur.x) / sx;
                float dy = (target_screen.y - cur.y) / sy;

                // без этого мышь улетает
                if (dx > 80.f) dx = 80.f;
                if (dx < -80.f) dx = -80.f;
                if (dy > 80.f) dy = 80.f;
                if (dy < -80.f) dy = -80.f;

                int mx = (int)std::lround(dx);
                int my = (int)std::lround(dy);
                if (mx == 0 && my == 0)
                {
                    return;
                }

                POINT screen{};
                RECT bounds{};
                if (GetCursorPos(&screen) && game_client_screen_rect(bounds))
                {
                    LONG pad = 2;
                    LONG left = bounds.left + pad;
                    LONG top = bounds.top + pad;
                    LONG right = bounds.right - pad - 1;
                    LONG bottom = bounds.bottom - pad - 1;
                    if (right < left) right = left;
                    if (bottom < top) bottom = top;

                    LONG nx = screen.x + mx;
                    LONG ny = screen.y + my;
                    if (nx < left) nx = left;
                    if (nx > right) nx = right;
                    if (ny < top) ny = top;
                    if (ny > bottom) ny = bottom;

                    mx = (int)(nx - screen.x);
                    my = (int)(ny - screen.y);
                    if (mx == 0 && my == 0)
                    {
                        return;
                    }
                }

                INPUT in{};
                in.type = INPUT_MOUSE;
                in.mi.dx = mx;
                in.mi.dy = my;
                in.mi.dwFlags = MOUSEEVENTF_MOVE;
                SendInput(1, &in, sizeof(in));
            }

            void apply_camera(const Config& cfg, const Scene& sc, const Vector3& target_world)
            {
                if (!g_Memory.IsValid(sc.camera.address))
                {
                    return;
                }

                Vector3 want = (target_world - sc.cam_pos);
                if (want.LengthSquared() < 1e-6f)
                {
                    return;
                }
                want.Normalize();

                Matrix4x4 cur = sc.camera.GetRotation();
                Vector3 cur_look(-cur.m[0][2], -cur.m[1][2], -cur.m[2][2]);
                if (cur_look.LengthSquared() < 1e-6f)
                {
                    cur_look = want;
                }
                cur_look.Normalize();

                float sx = cfg.smooth_x;
                float sy = cfg.smooth_y;
                if (sx < 0.1f) sx = 0.1f;
                if (sy < 0.1f) sy = 0.1f;

                float tx = 1.0f / sx;
                float ty = 1.0f / sy;
                float tz = (tx + ty) * 0.5f;

                Vector3 look(
                    cur_look.x + (want.x - cur_look.x) * tx,
                    cur_look.y + (want.y - cur_look.y) * ty,
                    cur_look.z + (want.z - cur_look.z) * tz);
                if (look.LengthSquared() < 1e-6f)
                {
                    return;
                }
                look.Normalize();

                Vector3 world_up(0.0f, 1.0f, 0.0f);
                Vector3 right = look.Cross(world_up);
                if (right.LengthSquared() < 1e-6f)
                {
                    right = Vector3(1.0f, 0.0f, 0.0f);
                }
                right.Normalize();
                Vector3 up = right.Cross(look);
                Vector3 back = -look;

                Matrix4x4 rot;
                rot.m[0][0] = right.x; rot.m[0][1] = up.x; rot.m[0][2] = back.x;
                rot.m[1][0] = right.y; rot.m[1][1] = up.y; rot.m[1][2] = back.y;
                rot.m[2][0] = right.z; rot.m[2][1] = up.z; rot.m[2][2] = back.z;
                sc.camera.SetRotation(rot);
            }

        }

        void Aim::Render()
        {
			Config& aim_cfg =
				(g_Settings.aim.type == 1) ? g_Settings.aim.camera : g_Settings.aim.mouse;
			Config& silent_cfg = g_Settings.aim.silent;

            // один слот BoundFunc, magic и raycast шарят RaycastSilent
            // (два хука на слот = Install thrash, работает 1 из 10).
            if (g_Settings.aim.silent_uses_raycast_hook())
            {
                if (MagicBullet::Ready())
                {
                    MagicBullet::Ensure(false);
                }
                if (!RaycastSilent::Ready())
                {
                    RaycastSilent::Ensure(true);
                }
            }

            else
            {
                MagicBullet::Ensure(false);
                RaycastSilent::Ensure(false);
            }

			if (g_Settings.aim.type != 2)
				draw_fov(aim_cfg);
			draw_fov(silent_cfg);

			// aim key
			bool aim_on = false;
			if (g_Settings.aim.type != 2)
			{
				if (g_Settings.aim.bind_mode == 2)
				{
					aim_on = true;
					s_was_pressed = false;
				}

				else if (g_Settings.aim.bind != 0)
				{
					bool pressed = (GetAsyncKeyState(g_Settings.aim.bind) & 0x8000) != 0;
					if (g_Settings.aim.bind_mode == 1)
					{
						if (pressed && !s_was_pressed)
							s_toggled = !s_toggled;
						aim_on = s_toggled;
					}

					else
					{
						aim_on = pressed;
					}
					s_was_pressed = pressed;
				}

				else
				{
					s_was_pressed = false;
					s_toggled = false;
				}
			}

			else
			{
				s_was_pressed = false;
				s_toggled = false;
			}

			// silent key (0 = тот же aim.bind, шарим состояние)
			bool silent_on = false;
			{
				int sk = g_Settings.aim.silent_bind;
				int sm = g_Settings.aim.silent_bind_mode;
				if (sk == 0)
				{
					sk = g_Settings.aim.bind;
					sm = g_Settings.aim.bind_mode;
				}

				if (sk != 0 && g_Settings.aim.silent_bind == 0 &&
				    g_Settings.aim.type != 2)
				{
					// один бинд с аимом — не дёргаем второй toggle
					silent_on = aim_on;
					s_silent_was = s_was_pressed;
					s_silent_tog = s_toggled;
				}

				else if (sm == 2)
				{
					silent_on = true;
					s_silent_was = false;
				}

				else if (sk != 0)
				{
					bool pressed = (GetAsyncKeyState(sk) & 0x8000) != 0;
					if (sm == 1)
					{
						if (pressed && !s_silent_was)
							s_silent_tog = !s_silent_tog;
						silent_on = s_silent_tog;
					}

					else
					{
						silent_on = pressed;
					}
					s_silent_was = pressed;
				}

				else
				{
					s_silent_was = false;
					s_silent_tog = false;
				}
			}

            if (!aim_on && !silent_on)
            {
                s_target = 0;
                s_pending = 0;
                RaycastSilent::SetActive(false);
                MagicBullet::SetActive(false);
                clear_silents();
                release_cursor_clip();
                return;
            }

            Scene sc = resolve_scene();
            if (!sc.ok)
            {
                s_target = 0;
                RaycastSilent::SetActive(false);
                MagicBullet::SetActive(false);
                clear_silents();
                release_cursor_clip();
                return;
            }

			// таргет: если оба — aim cfg, иначе кто активен
			Config& tgt_cfg = aim_on ? aim_cfg : silent_cfg;

            Vector3 world{};
            Vector2 screen{};
            if (!select_target(tgt_cfg, sc, world, screen))
            {
                s_target = 0;
                RaycastSilent::SetActive(false);
                MagicBullet::SetActive(false);
                clear_silents();
                release_cursor_clip();
                return;
            }

            draw_aim_tracer(tgt_cfg, screen);

			bool aim_fire = aim_on && hitchance_ok(aim_cfg);
			bool silent_fire = silent_on && hitchance_ok(silent_cfg);

			if (!aim_fire && !silent_fire)
			{
				RaycastSilent::SetActive(false);
				MagicBullet::SetActive(false);
				clear_silents();
				release_cursor_clip();
				return;
			}

			if (aim_fire && g_Settings.aim.type == 0)
			{
				apply_mouse(aim_cfg, screen);
			}

			else if (aim_fire && g_Settings.aim.type == 1)
			{
				release_cursor_clip();
				apply_camera(aim_cfg, sc, world);
			}

			else if (!aim_fire)
			{
				release_cursor_clip();
			}

            if (silent_fire &&
                g_Settings.aim.silent_method == Settings::SILENT_VIEWPORT)
            {
                release_cursor_clip();
                RaycastSilent::SetActive(false);
                MagicBullet::SetActive(false);
                MouseSilent::SetActive(false);
                ViewportSilent::SetActive(true, world);
            }

            else if (silent_fire &&
                     g_Settings.aim.silent_method == Settings::SILENT_MOUSE)
            {
                release_cursor_clip();
                RaycastSilent::SetActive(false);
                MagicBullet::SetActive(false);
                ViewportSilent::SetActive(false);
                MouseSilent::SetActive(true, world);
            }

            else if (silent_fire &&
                     g_Settings.aim.silent_method == Settings::SILENT_RAYCAST)
            {
                release_cursor_clip();
                ViewportSilent::SetActive(false);
                MouseSilent::SetActive(false);
                PhantomSilent::SetActive(false);

                // force magic поверх raycast
                bool force_mb = false;
                int fk = g_Settings.aim.force_magic_key;
                if (g_Settings.aim.force_magic_mode == 2)
                {
                    force_mb = true;
                    s_force_mb_was = false;
                    g_Settings.aim.force_magic_bullet = true;
                }

                else if (fk != 0)
                {
                    bool fdown = (GetAsyncKeyState(fk) & 0x8000) != 0;
                    if (g_Settings.aim.force_magic_mode == 1)
                    {
                        if (fdown && !s_force_mb_was)
                        {
                            s_force_mb_tog = !s_force_mb_tog;
                        }
                        force_mb = s_force_mb_tog;
                    }

                    else
                    {
                        force_mb = fdown;
                    }
                    s_force_mb_was = fdown;
                    g_Settings.aim.force_magic_bullet = force_mb;
                }

                else if (g_Settings.aim.force_magic_bullet)
                {
                    force_mb = true;
                    s_force_mb_was = false;
                }

                else
                {
                    s_force_mb_was = false;
                    s_force_mb_tog = false;
                }

                MagicBullet::SetActive(false);
                RaycastSilent::SetActive(true, world, force_mb);
            }

            else if (silent_fire &&
                     g_Settings.aim.silent_method == Settings::SILENT_MAGIC_BULLET)
            {
                release_cursor_clip();
                ViewportSilent::SetActive(false);
                MouseSilent::SetActive(false);
                PhantomSilent::SetActive(false);
                MagicBullet::SetActive(false);
                // wallbang, camera-ray stub скипает (origin ~ cam)
                RaycastSilent::SetActive(true, world, true);
            }

            else if (silent_fire &&
                     g_Settings.aim.silent_method == Settings::SILENT_PHANTOM)
            {
                release_cursor_clip();
                ViewportSilent::SetActive(false);
                MouseSilent::SetActive(false);
                RaycastSilent::SetActive(false);
                MagicBullet::SetActive(false);
                PhantomSilent::SetActive(true, world);
            }

            else
            {
                clear_silents();
                RaycastSilent::SetActive(false);
                MagicBullet::SetActive(false);
            }
        }

        std::uint64_t Aim::CurrentTarget() { return s_target; }
        int Aim::CurrentAimPart() { return s_aim_part; }
        Vector3 Aim::CurrentAimPoint() { return s_aim_point; }

    }
}
