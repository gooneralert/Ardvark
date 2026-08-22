#include "pch.h"
#include "Triggerbot.h"

#include "app/Settings.h"
#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/player/PlayerHandler.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"
#include "features/games/PhantomForces.h"
#include "features/GameCursor.h"
#include "features/visuals/RaycastEngine.h"
#include "renderer/Renderer.h"
#include "gui/Menu.h"
#include "imgui.h"

#include <windows.h>
#include <cmath>
#include <cfloat>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <chrono>

namespace Cheat {
namespace Features {

namespace {

// ---------------------------------------------------------------- input io --

// gelato _input_key_gate — режимы 0 hold / 1 toggle / 2 always / 3 once
struct key_gate_t {
    bool active = false;
    bool just_activated = false;
};

bool gate_key_down(int key)
{
    return key > 0 && (GetAsyncKeyState(key) & 0x8000) != 0;
}

key_gate_t key_gate(int key, int mode, bool& toggled, bool& was_down)
{
    if (mode == 2)
    {
        const bool just = !was_down;
        was_down = true;
        return { true, just };
    }

    if (key <= 0)
    {
        was_down = false;
        return { false, false };
    }

    const bool down = gate_key_down(key);

    if (mode == 1)
    {
        bool just = false;
        if (down && !was_down)
        {
            toggled = !toggled;
            just = toggled;
        }
        was_down = down;
        return { toggled, just };
    }

    if (mode == 3)
    {
        const bool just = down && !was_down;
        was_down = down;
        return { just, just };
    }

    const bool just = down && !was_down;
    was_down = down;
    return { down, just };
}

// курсор — ровно как в gelato (_input_cursor): GetCursorPos + ScreenToClient
// на окне WINDOWSCLIENT. При mouse-lock (first person) системный курсор держится
// в центре WINDOWSCLIENT, поэтому позиция всегда в нужном месте и не «уезжает».
bool cursor_screen(Vector2& out)
{
    float x = 0.f, y = 0.f;
    if (GameCursor::Cursor(x, y))
    {
        out = Vector2(x, y);
        return true;
    }

    // fallback: окно, к которому прикреплён оверлей
    POINT point{};
    HWND hwnd = Renderer::GetHwnd();
    if (!hwnd)
        hwnd = Renderer::GetGameHwnd();
    if (hwnd && GetCursorPos(&point) && ScreenToClient(hwnd, &point))
    {
        out = Vector2(static_cast<float>(point.x), static_cast<float>(point.y));
        return true;
    }

    // последний fallback: imgui
    const ImVec2 io = ImGui::GetIO().MousePos;
    if (io.x > -1e6f && io.y > -1e6f && io.x < 1e9f && io.y < 1e9f)
    {
        out = Vector2(io.x, io.y);
        return true;
    }

    return false;
}

void click_down()
{
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    SendInput(1, &input, sizeof(INPUT));
}

void click_up()
{
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &input, sizeof(INPUT));
}

// ------------------------------------------------------------- view / world --

bool resolve_view(Matrix4x4& out_view, Vector2& out_dims)
{
    if (!Globals::Workspace || !g_Memory.IsValid(Globals::Workspace->address))
        return false;

    auto cam_ptr = Globals::Workspace->GetCurrentCamera();
    if (!cam_ptr || !g_Memory.IsValid(cam_ptr->address))
        return false;

    Camera cam(cam_ptr->address);
    out_dims = cam.GetViewportSize();
    if (out_dims.x < 1.f || out_dims.y < 1.f)
        return false;

    static uintptr_t base = 0;
    if (!base)
        base = g_Memory.GetModuleBase();
    if (!base)
        return false;

    uintptr_t ve = g_Memory.Read<uintptr_t>(base + ::VisualEngine::Pointer);
    if (!g_Memory.IsValid(ve))
        return false;

    out_view = g_Memory.Read<Matrix4x4>(ve + ::VisualEngine::ViewMatrix);
    return true;
}

// как в AimTarget: проекция с масштабированием под оверлей
bool world_to_screen(const Matrix4x4& m, const Vector2& dim, const Vector3& p, Vector2& out)
{
    float w = p.x * m.m[3][0] + p.y * m.m[3][1] + p.z * m.m[3][2] + m.m[3][3];
    if (w < 0.01f)
        return false;

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
            out.x *= static_cast<float>(ocr.right - ocr.left) / dim.x;
            out.y *= static_cast<float>(ocr.bottom - ocr.top) / dim.y;
        }
    }
    return true;
}

Vector3 camera_position()
{
    if (!Globals::Workspace || !g_Memory.IsValid(Globals::Workspace->address))
        return {};

    auto cam_ptr = Globals::Workspace->GetCurrentCamera();
    if (!cam_ptr || !g_Memory.IsValid(cam_ptr->address))
        return {};

    return Camera(cam_ptr->address).GetPosition();
}

std::uint64_t local_player_addr()
{
    if (Globals::Players && g_Memory.IsValid(Globals::Players->address))
    {
        const std::uint64_t lp = g_Memory.Read<std::uint64_t>(
            Globals::Players->address + ::Player::LocalPlayer);
        if (g_Memory.IsValid(lp))
            return lp;
    }
    return 0;
}

std::uint64_t local_character_addr()
{
    if (Globals::Players && g_Memory.IsValid(Globals::Players->address))
    {
        const std::uint64_t lp = g_Memory.Read<std::uint64_t>(
            Globals::Players->address + ::Player::LocalPlayer);
        if (g_Memory.IsValid(lp))
            return g_Memory.Read<std::uint64_t>(lp + ::Player::ModelInstance);
    }
    return 0;
}

// ------------------------------------------------------------------- parts --

struct part_info_t {
    bool valid = false;
    std::uint64_t addr = 0;
    Vector3 position{};
    Vector3 size{};
    Matrix4x4 rotation{};
    Vector3 velocity{};
};

bool read_slot(const std::shared_ptr<Instance>& part, part_info_t& out)
{
    if (!part || !g_Memory.IsValid(part->address))
        return false;

    BasePart bp(part->address);
    out.addr = part->address;
    out.position = bp.GetPosition();
    out.size = bp.GetSize();
    out.rotation = bp.GetRotation();
    out.velocity = bp.GetAssemblyLinearVelocity();

    // валидность только по позиции — для PF у парт size часто ~0,
    // размер подставляем на этапе пика (как в ESP)
    out.valid =
        std::isfinite(out.position.x) && std::isfinite(out.position.y) && std::isfinite(out.position.z) &&
        !(out.position.x == 0.0f && out.position.y == 0.0f && out.position.z == 0.0f);
    return out.valid;
}

// все слоты тела игрока (r15/r6-маппинг как в update.h PopulatePartsFromChildren)
void refresh_body_slots(const PlayerCache& c, const std::shared_ptr<Instance>* out[16], int idx_out[16])
{
    out[0] = &c.head;                    idx_out[0] = 0;
    out[1] = &c.humanoidRootPart;        idx_out[1] = 1;
    out[2] = &c.upperTorso;              idx_out[2] = 2;
    out[3] = &c.lowerTorso;              idx_out[3] = 3;
    out[4] = &c.leftUpperArm;            idx_out[4] = 4;
    out[5] = &c.leftLowerArm;            idx_out[5] = 5;
    out[6] = &c.leftHand;                idx_out[6] = 6;
    out[7] = &c.rightUpperArm;           idx_out[7] = 7;
    out[8] = &c.rightLowerArm;           idx_out[8] = 8;
    out[9] = &c.rightHand;               idx_out[9] = 9;
    out[10] = &c.leftUpperLeg;           idx_out[10] = 10;
    out[11] = &c.leftLowerLeg;           idx_out[11] = 11;
    out[12] = &c.leftFoot;               idx_out[12] = 12;
    out[13] = &c.rightUpperLeg;          idx_out[13] = 13;
    out[14] = &c.rightLowerLeg;          idx_out[14] = 14;
    out[15] = &c.rightFoot;              idx_out[15] = 15;
}

const char* slot_name_for_idx(int idx)
{
    static const char* names[16] = {
        "head", "hrp", "upperTorso", "lowerTorso",
        "lUpperArm", "lLowerArm", "lHand",
        "rUpperArm", "rLowerArm", "rHand",
        "lUpperLeg", "lLowerLeg", "lFoot",
        "rUpperLeg", "rLowerLeg", "rFoot"
    };
    if (idx >= 0 && idx < 16)
        return names[idx];
    return "?";
}

int slot_idx_for_ptr(const std::shared_ptr<Instance>* const slots[16], const std::shared_ptr<Instance>* slot)
{
    for (int i = 0; i < 16; ++i)
        if (slots[i] == slot)
            return i;
    return -1;
}

// 8 углов OBB парты, как _part_corners в gelato esp/helpers.h
void part_corners(const part_info_t& p, Vector3 corners[8])
{
    const Vector3 half(p.size.x * 0.5f, p.size.y * 0.5f, p.size.z * 0.5f);

    const float sx[8] = { -half.x, -half.x, -half.x, -half.x,  half.x,  half.x,  half.x,  half.x };
    const float sy[8] = { -half.y, -half.y,  half.y,  half.y, -half.y, -half.y,  half.y,  half.y };
    const float sz[8] = { -half.z,  half.z, -half.z,  half.z, -half.z,  half.z, -half.z,  half.z };

    for (int i = 0; i < 8; ++i)
    {
        const Vector3 local(sx[i], sy[i], sz[i]);
        const Vector3 rotated(
            p.rotation.m[0][0] * local.x + p.rotation.m[0][1] * local.y + p.rotation.m[0][2] * local.z,
            p.rotation.m[1][0] * local.x + p.rotation.m[1][1] * local.y + p.rotation.m[1][2] * local.z,
            p.rotation.m[2][0] * local.x + p.rotation.m[2][1] * local.y + p.rotation.m[2][2] * local.z);
        corners[i] = Vector3(
            p.position.x + rotated.x,
            p.position.y + rotated.y,
            p.position.z + rotated.z);
    }
}

// курсор внутри проекции бокса парты (как cursor_on_part в gelato)
bool cursor_on_part(
    const part_info_t& p,
    const Matrix4x4& view,
    const Vector2& dims,
    const Vector2& cursor)
{
    if (p.size.x <= 0.0f || p.size.y <= 0.0f || p.size.z <= 0.0f)
        return false;

    float min_x = FLT_MAX, min_y = FLT_MAX;
    float max_x = -FLT_MAX, max_y = -FLT_MAX;
    int projected = 0;

    Vector3 corners[8];
    part_corners(p, corners);
    for (int i = 0; i < 8; ++i)
    {
        Vector2 screen{};
        if (!world_to_screen(view, dims, corners[i], screen))
            continue;

        min_x = (std::min)(min_x, screen.x);
        min_y = (std::min)(min_y, screen.y);
        max_x = (std::max)(max_x, screen.x);
        max_y = (std::max)(max_y, screen.y);
        projected++;
    }

    if (projected == 0)
        return false;

    return cursor.x >= min_x && cursor.x <= max_x &&
           cursor.y >= min_y && cursor.y <= max_y;
}

// слоты тела для конкретной группы: 0 whole, 1 head, 2 torso, 3 arms,
// 4 legs, 5 hrp (как hitbox_for_group в gelato, но с учётом всех слотов)
int group_slots(const PlayerCache& c, int part, const std::shared_ptr<Instance>* out[8])
{
    int n = 0;
    auto add = [&](const std::shared_ptr<Instance>* s) {
        if (s && *s && g_Memory.IsValid((*s)->address) && n < 8)
            out[n++] = s;
    };

    switch (part)
    {
    case Settings::TRIGGER_HITBOX_HEAD:
        add(&c.head);
        break;

    case Settings::TRIGGER_HITBOX_HRP:
        add(&c.humanoidRootPart);
        break;

    case Settings::TRIGGER_HITBOX_TORSO:
    {
        // как в gelato: UpperTorso/Torso -> LowerTorso -> HumanoidRootPart
        add(&c.upperTorso);
        add(&c.lowerTorso);
        add(&c.humanoidRootPart);
        break;
    }

    case Settings::TRIGGER_HITBOX_ARMS:
    {
        add(&c.leftUpperArm);
        add(&c.leftLowerArm);
        add(&c.leftHand);
        add(&c.rightUpperArm);
        add(&c.rightLowerArm);
        add(&c.rightHand);
        break;
    }

    case Settings::TRIGGER_HITBOX_LEGS:
    {
        add(&c.leftUpperLeg);
        add(&c.leftLowerLeg);
        add(&c.leftFoot);
        add(&c.rightUpperLeg);
        add(&c.rightLowerLeg);
        add(&c.rightFoot);
        break;
    }

    default:
        break;
    }
    return n;
}

// выбор парты под курсором — как pf_part_under_cursor из gelato:
// кандидат ОБЯЗАН быть под прицелом (cursor_on_part), из таких берём
// ближайший к центру. не-PF смотрит только группу hitbox'ов, PF-режим
// дополнительно перебирает все слоты тела.
struct picked_part_t {
    const std::shared_ptr<Instance>* slot = nullptr;
    part_info_t info{};
    int idx = -1;
    float dist_screen = FLT_MAX;
};

bool pick_part(
    const PlayerCache& c,
    const Matrix4x4& view,
    const Vector2& dims,
    const Vector2& cursor,
    int hitbox,
    bool pf_mode,
    float mult,
    picked_part_t& out)
{
    const std::shared_ptr<Instance>* slots[16];
    int slot_idx[16];
    refresh_body_slots(c, slots, slot_idx);

    auto consider = [&](const std::shared_ptr<Instance>* slot) {
        if (!slot || !*slot || !g_Memory.IsValid((*slot)->address))
            return;

        part_info_t info{};
        if (!read_slot(*slot, info))
            return;

        // PF: реальный size парт часто ~0 — подставляем R6 габариты (как в ESP)
        if (pf_mode &&
            info.size.x < 0.01f && info.size.y < 0.01f && info.size.z < 0.01f)
        {
            if (c.head && *slot == c.head)
                info.size = Vector3(1.f, 1.f, 1.f);
            else if ((c.upperTorso && *slot == c.upperTorso) ||
                     (c.humanoidRootPart && *slot == c.humanoidRootPart))
                info.size = Vector3(2.f, 2.f, 1.f);
            else
                info.size = Vector3(1.f, 2.f, 1.f);
        }

        // hitbox multiplier — расширяем область срабатывания (не сами парты)
        if (mult > 0.001f)
            info.size = Vector3(info.size.x * mult, info.size.y * mult, info.size.z * mult);

        // не под прицелом — пропускаем (иначе стреляем «в никуда»)
        if (!cursor_on_part(info, view, dims, cursor))
            return;

        Vector2 sp{};
        if (!world_to_screen(view, dims, info.position, sp))
            return;

        const float dx = sp.x - cursor.x;
        const float dy = sp.y - cursor.y;
        const float dist = dx * dx + dy * dy;
        if (dist < out.dist_screen)
        {
            out.dist_screen = dist;
            out.slot = slot;
            out.info = info;
            out.idx = slot_idx_for_ptr(slots, slot);
        }
    };

    if (pf_mode || hitbox == Settings::TRIGGER_HITBOX_WHOLE)
    {
        for (int i = 0; i < 16; ++i)
            consider(slots[i]);
    }
    else
    {
        const std::shared_ptr<Instance>* by[8];
        const int n = group_slots(c, hitbox, by);
        for (int i = 0; i < n; ++i)
            consider(by[i]);
    }

    return out.slot != nullptr;
}

// ------------------------------------------------------------ player checks --

// является ли запись кэша локалом — кэш ключуется по-разному:
//   - workspace scan:      cache.address == character model
//   - Players path:        cache.address == Player instance, cache.character == model
//   - PF/bots:             cache.player_address / cache.character могут быть локалом
// Проверяем все четыре, иначе бот стреляет по самому себе.
bool is_local_cache(const PlayerCache& c, std::uint64_t local_player, std::uint64_t local_char)
{
    return (local_player && (c.address == local_player || c.player_address == local_player)) ||
           (local_char && (c.address == local_char || c.character == local_char));
}

bool check_invisible(const PlayerCache& c)
{
    const std::shared_ptr<Instance>* targets[2] = { &c.head, &c.humanoidRootPart };
    for (auto* s : targets)
    {
        if (s && *s && g_Memory.IsValid((*s)->address))
        {
            if (BasePart((*s)->address).GetTransparency() >= 0.95f)
                return true;
        }
    }
    return false;
}

// ForceField чайлд в чаре — gelato считал это armor > 0
bool check_forcefield(const PlayerCache& c)
{
    if (!g_Memory.IsValid(c.character))
        return false;

    for (const auto& child : Instance(c.character).GetChildren())
    {
        if (child.GetClassName() == "ForceField")
            return true;
    }
    return false;
}

bool passes_checks(const PlayerCache& cache, int checks, std::uint64_t local_team)
{
    if (checks & Settings::TRIGGER_CHECK_TEAM)
    {
        if (local_team != 0 && PlayerHandler::IsTeammate(cache, local_team))
            return false;
    }

    if (checks & Settings::TRIGGER_CHECK_HEALTH)
    {
        if (cache.is_corpse)
            return false;
        if (cache.humanoid && g_Memory.IsValid(cache.humanoid->address))
        {
            Humanoid hum(cache.humanoid->address);
            if (hum.GetStateId() == 15 || hum.GetHealth() <= 0.0f)
                return false;
        }
    }

    if (checks & Settings::TRIGGER_CHECK_FF)
    {
        if (check_forcefield(cache))
            return false;
    }

    if (checks & Settings::TRIGGER_CHECK_INVIS)
    {
        if (check_invisible(cache))
            return false;
    }

    return true;
}

// ---------------------------------------------------------- prediction/rng --

Vector3 lead_target(const Vector3& shooter, const Vector3& target, const Vector3& vel,
                    const Vector3& mult)
{
    const Vector3 to_target = target - shooter;
    const float dist = to_target.Length();
    if (dist < 1e-3f)
        return target;

    // gelato вызывал lead_target со скоростью снаряда 0 → ветка t = dist/2000
    const float t = dist / 2000.0f;
    return Vector3(
        target.x + vel.x * t * mult.x,
        target.y + vel.y * t * mult.y,
        target.z + vel.z * t * mult.z);
}

bool roll_hitchance(float percent, std::uint64_t seed)
{
    if (percent >= 100.0f)
        return true;
    if (percent <= 0.0f)
        return false;

    std::uint32_t x = static_cast<std::uint32_t>(seed ^ 0x9e3779b9u);
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    const float roll = (static_cast<float>(x) / 4294967295.0f) * 100.0f;
    return roll <= percent;
}

// --------------------------------------------------------- state machine io --
// стейт клика как в gelato triggerbot

bool s_locked = false;
bool s_holding = false;
bool s_armed = false;
bool s_pending_release = false;
std::uint64_t s_last_fired = 0;
std::chrono::steady_clock::time_point s_arm_at{};
std::chrono::steady_clock::time_point s_release_at{};

void reset_state()
{
    s_locked = false;
    if (s_holding)
        click_up();
    s_holding = false;
    s_armed = false;
    s_pending_release = false;
    s_last_fired = 0;
}

// точная копия стейт-машины gelato on_worker_tick (delay → click → release)
void tick_state(bool ready, std::uint64_t fire_key, float delay_ms, float click_duration_ms)
{
    const int delay = static_cast<int>(std::lround(std::clamp(delay_ms, 0.0f, 300.0f)));
    const int duration = static_cast<int>(std::lround(std::clamp(click_duration_ms, 0.0f, 500.0f)));
    const auto now = std::chrono::steady_clock::now();

    if (!ready)
    {
        if (s_holding)
        {
            if (!s_pending_release)
            {
                s_release_at = now + std::chrono::milliseconds(duration);
                s_pending_release = true;
            }
            else if (now >= s_release_at)
            {
                click_up();
                s_holding = false;
                s_pending_release = false;
            }
        }
        else
        {
            s_armed = false;
            s_last_fired = 0;
        }
        return;
    }

    if (!s_holding)
    {
        s_pending_release = false;
        if (fire_key != s_last_fired)
        {
            s_last_fired = fire_key;
            s_arm_at = now + std::chrono::milliseconds(delay);
            s_armed = true;
        }

        if (s_armed && now >= s_arm_at)
        {
            click_down();
            s_holding = true;
            s_release_at = now + std::chrono::milliseconds(duration);
            s_pending_release = true;
            s_armed = false;
        }
        return;
    }

    if (s_pending_release && now >= s_release_at)
    {
        click_up();
        s_pending_release = false;
        s_arm_at = now + std::chrono::milliseconds(delay);
        s_armed = true;
    }
}

// ---------------------------------------------------------- debug hud --//

struct debug_draw_t {
    bool view_ok = false;
    bool cursor_ok = false;
    Vector2 cursor{};
    Vector2 dims{};
    Matrix4x4 view{};
    bool menu_capture = false;
    bool pf_parts = false;
    int players = 0;
    std::uint64_t best_addr = 0;
    std::uint64_t local_player = 0;
    std::uint64_t local_char = 0;
    // геометрия залоченной парты (для отрисовки бокса на экране)
    bool has_best_geo = false;
    int best_idx = -1;
    Vector3 best_world{};
    Vector3 best_size{};
    Matrix4x4 best_rot{};
    const char* best_name = "";
    float best_px = 0.f;
    const char* stage = "idle";
};

// экранная отладочная панель: реальное положение курсора (как его видит
// ардварк) жёлтым крестом + все гейты/статусы текстом.
void draw_debug_hud(const Settings::TriggerbotConfig& cfg, const key_gate_t& gate,
                    const debug_draw_t& dbg)
{
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl)
        return;

    // проекция 8 углов залоченной парты — показываем бокс, по которому бот целится
    if (dbg.has_best_geo && dbg.view_ok)
    {
        part_info_t p;
        p.position = dbg.best_world;
        p.size = dbg.best_size;
        p.rotation = dbg.best_rot;

        Vector3 corners[8];
        part_corners(p, corners);

        Vector2 sc[8];
        int n = 0;
        for (int i = 0; i < 8; ++i)
        {
            if (world_to_screen(dbg.view, dbg.dims, corners[i], sc[n]))
                n++;
        }

        if (n > 0)
        {
            float min_x = FLT_MAX, min_y = FLT_MAX, max_x = -FLT_MAX, max_y = -FLT_MAX;
            for (int i = 0; i < n; ++i)
            {
                min_x = (std::min)(min_x, sc[i].x);
                min_y = (std::min)(min_y, sc[i].y);
                max_x = (std::max)(max_x, sc[i].x);
                max_y = (std::max)(max_y, sc[i].y);
            }

            const bool cursor_inside =
                dbg.cursor.x >= min_x && dbg.cursor.x <= max_x &&
                dbg.cursor.y >= min_y && dbg.cursor.y <= max_y;

            // зелёный = курсор внутри бокса (готов стрелять), красный = бокс мимо
            const ImU32 col = cursor_inside ? IM_COL32(0, 255, 0, 255) : IM_COL32(255, 40, 40, 255);

            dl->AddRect(ImVec2(min_x, min_y), ImVec2(max_x, max_y), col, 0.f, 0, 2.f);

            // размер бокса в пикселях + попадание
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%s [%d]  box=%dx%dpx  in=%d",
                dbg.best_name, dbg.best_idx,
                (int)(max_x - min_x), (int)(max_y - min_y), (int)cursor_inside);
            dl->AddText(ImVec2(min_x, min_y - 14.f), IM_COL32(255, 255, 255, 240), buf);
        }
    }

    if (dbg.cursor_ok)
    {
        const ImU32 col = IM_COL32(255, 230, 0, 255);
        dl->AddLine(ImVec2(dbg.cursor.x - 9.f, dbg.cursor.y), ImVec2(dbg.cursor.x + 9.f, dbg.cursor.y), col, 1.6f);
        dl->AddLine(ImVec2(dbg.cursor.x, dbg.cursor.y - 9.f), ImVec2(dbg.cursor.x, dbg.cursor.y + 9.f), col, 1.6f);
        dl->AddCircle(ImVec2(dbg.cursor.x, dbg.cursor.y), 4.f, col, 0, 1.f);
    }

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.f, 1.f, 0.f, 0.8f));
    ImGui::SetNextWindowPos(ImVec2(8.f, 8.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300.f, 0.f), ImGuiCond_Always);
    ImGui::Begin("##trig_debug", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoInputs);
    {
        ImGui::TextColored(ImVec4(1.f, 1.f, 0.f, 1.f), "TRIGGERBOT DEBUG");
        ImGui::Text("enabled=%d key=%d(0x%X) mode=%d key_gate=%d",
                    cfg.enabled, cfg.key, cfg.key, cfg.key_mode, gate.active);
        ImGui::Text("view_ok=%d  cursor_ok=%d  menu_capture=%d",
                    dbg.view_ok, dbg.cursor_ok, dbg.menu_capture);

        const ImVec2 io = ImGui::GetIO().MousePos;
        ImGui::Text("os-cursor == (%.0f, %.0f)", dbg.cursor.x, dbg.cursor.y);
        ImGui::Text("imgui-cursor = (%.0f, %.0f)", io.x, io.y);
        ImGui::Text("viewport=(%.0f, %.0f)  pf_parts=%d",
                    dbg.dims.x, dbg.dims.y, (int)dbg.pf_parts);
        ImGui::Text("players=%d  best=%llX", dbg.players, (unsigned long long)dbg.best_addr);
        ImGui::Text("localP=%llX  localC=%llX",
                    (unsigned long long)dbg.local_player, (unsigned long long)dbg.local_char);
        ImGui::Text("best_part=%s  px=%.0f", dbg.best_name, dbg.best_px);
        ImGui::Text("locked=%d holding=%d armed=%d last=%llX",
                    s_locked, s_holding, s_armed, (unsigned long long)s_last_fired);
        ImGui::Text("stage: %s", dbg.stage);
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
}

} // namespace

// ------------------------------------------------------------------ render --

bool Triggerbot::HasTarget()
{
    return s_locked;
}

void Triggerbot::Reset()
{
    reset_state();
}

void Triggerbot::Render()
{
    Settings::TriggerbotConfig& cfg = g_Settings.triggerbot;

    debug_draw_t dbg;
    dbg.players = (int)PlayerHandler::GetPlayerCount();

    static bool gate_toggled = false;
    static bool gate_was_down = false;
    const key_gate_t gate = key_gate(cfg.key, cfg.key_mode, gate_toggled, gate_was_down);

    auto draw_dbg = [&](const char* stage) {
        dbg.stage = stage;
        if (cfg.debug)
            draw_debug_hud(cfg, gate, dbg);
    };

    if (!cfg.enabled || !gate.active)
    {
        draw_dbg(!cfg.enabled ? "disabled" : "no key");
        reset_state();
        return;
    }

    Matrix4x4 view{};
    Vector2 dims{};
    if (!resolve_view(view, dims))
    {
        draw_dbg("no view");
        reset_state();
        return;
    }
    dbg.view_ok = true;
    dbg.dims = dims;
    dbg.view = view;

    Vector2 cursor{};
    dbg.cursor_ok = cursor_screen(cursor);
    dbg.cursor = cursor;
    if (!dbg.cursor_ok)
    {
        draw_dbg("no cursor");
        reset_state();
        return;
    }

    // не стреляем когда мышь занята меню/окошками оверлея
    dbg.menu_capture = GUI::Menu::ShouldCaptureMouse(cursor.x, cursor.y);
    if (dbg.menu_capture)
    {
        draw_dbg("menu capture");
        reset_state();
        return;
    }

    const Vector3 cam_pos = camera_position();
    const std::uint64_t local_player = local_player_addr();
    const std::uint64_t local_char = local_character_addr();
    dbg.local_player = local_player;
    dbg.local_char = local_char;
    const std::uint64_t local_team = PlayerHandler::LocalTeamFolder();
    const bool pf_parts = Games::PhantomForces::IsActivePlace() && cfg.pf_parts;
    dbg.pf_parts = pf_parts;

    std::uint64_t best_addr = 0;
    const std::shared_ptr<Instance>* best_slot = nullptr;
    float best_dist = FLT_MAX;

    PlayerHandler::ForEachPlayer([&](const PlayerCache& cache)
    {
        if (is_local_cache(cache, local_player, local_char))
            return;

        if (cache.address == 0 || !g_Memory.IsValid(cache.address))
            return;

        if (!passes_checks(cache, cfg.checks, local_team))
            return;

        if (cfg.max_distance > 0.0f)
        {
            const Vector3 part_pos = (cache.humanoidRootPart && g_Memory.IsValid(cache.humanoidRootPart->address))
                ? BasePart(cache.humanoidRootPart->address).GetPosition()
                : Vector3{};
            if (part_pos.x != 0.0f || part_pos.y != 0.0f || part_pos.z != 0.0f)
            {
                if (part_pos.DistanceTo(cam_pos) > cfg.max_distance)
                    return;
            }
        }

        picked_part_t tp{};
        if (!pick_part(cache, view, dims, cursor, cfg.hitboxes, pf_parts, cfg.hitbox_multiplier, tp))
            return;

        Vector3 target_world = tp.info.position;
        if (cfg.prediction_enabled)
        {
            const Vector3 mult(cfg.prediction_xz, cfg.prediction_y, cfg.prediction_xz);
            target_world = lead_target(cam_pos, tp.info.position, tp.info.velocity, mult);
        }

        if (cfg.checks & Settings::TRIGGER_CHECK_VISIBLE)
        {
            if (RaycastEngine::IsEnabled())
            {
                const auto vis = RaycastEngine::IsPlayerVisible(cache, cam_pos);
                if (!vis.visible)
                    return;
            }
        }

        Vector2 sp{};
        if (!world_to_screen(view, dims, target_world, sp))
            return;

        const float dx = sp.x - cursor.x;
        const float dy = sp.y - cursor.y;
        const float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < best_dist)
        {
            best_dist = dist;
            best_addr = cache.address;
            best_slot = tp.slot;

            dbg.has_best_geo = true;
            dbg.best_idx = tp.idx;
            dbg.best_world = tp.info.position;
            dbg.best_size = tp.info.size;
            dbg.best_rot = tp.info.rotation;
            dbg.best_name = slot_name_for_idx(tp.idx);
            dbg.best_px = dist;
        }
    });

    dbg.best_addr = best_addr;

    bool ready = best_addr != 0 && best_slot;

    // как в gelato: ролл меняет ready, но стейт-машина всё равно тикает
    // (нужно чтобы клик корректно «отпускался»)
    if (ready && cfg.hitchance < 100.0f)
        ready = roll_hitchance(cfg.hitchance, best_addr);

    s_locked = ready;

    const std::uint64_t fire_key = ready ? best_addr : 0;
    tick_state(ready, fire_key, cfg.delay_ms, cfg.click_duration_ms);

    draw_dbg(ready ? (s_holding ? "firing" : "on target") : "scanning");

    // индикатор под курсором, как render() в gelato (draw_prediction)
    if (cfg.draw_prediction)
    {
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (draw)
        {
            ImVec2 center = ImGui::GetIO().MousePos;
            if (center.x <= 0.f && center.y <= 0.f)
                center = ImVec2(dims.x * 0.5f, dims.y * 0.5f);

            if (s_locked)
            {
                if (s_holding)
                {
                    draw->AddCircle(center, 8.0f, IM_COL32(255, 50, 50, 220), 12, 2.0f);
                    draw->AddCircleFilled(center, 3.0f, IM_COL32(255, 50, 50, 200), 8);
                }
                else
                {
                    draw->AddCircle(center, 8.0f, IM_COL32(50, 255, 50, 180), 12, 2.0f);
                    draw->AddCircleFilled(center, 3.0f, IM_COL32(50, 255, 50, 160), 8);
                }
            }
        }
    }
}

} // namespace Features
} // namespace Cheat
