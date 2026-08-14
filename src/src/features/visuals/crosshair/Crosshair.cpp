#include "pch.h"
#include "Crosshair.h"

#include "app/Settings.h"
#include "imgui.h"
#include "renderer/Renderer.h"

#include <Windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>

#pragma comment(lib, "winmm.lib")

namespace Cheat {
namespace Visuals {
namespace Crosshair {

namespace {

std::atomic<bool> g_want_suppress{ false };
std::atomic<bool> g_thread_stop{ false };
HANDLE            g_thread = nullptr;
bool              g_atexit_registered = false;

HCURSOR g_blank_cursor = nullptr;

HWND    g_patched_hwnd = nullptr;
HCURSOR g_saved_class_cursor = nullptr;
bool    g_class_cursor_patched = false;

DWORD   g_attached_tid = 0;

ImU32 Col4(const float c[4])
{
    return IM_COL32(
        (int)(c[0] * 255), (int)(c[1] * 255),
        (int)(c[2] * 255), (int)(c[3] * 255));
}

ImVec2 OverlaySize()
{
    HWND hwnd = Renderer::GetHwnd();
    if (hwnd)
    {
        RECT rc{};
        if (GetClientRect(hwnd, &rc))
        {
            float w = (float)(rc.right - rc.left);
            float h = (float)(rc.bottom - rc.top);
            if (w < 1.f) w = 1.f;
            if (h < 1.f) h = 1.f;
            return ImVec2(w, h);
        }
    }

    return ImGui::GetIO().DisplaySize;
}

ImVec2 CursorClient()
{
    ImVec2 sz = OverlaySize();
    POINT p{};
    if (!GetCursorPos(&p))
        return ImVec2(sz.x * 0.5f, sz.y * 0.5f);

    HWND overlay = Renderer::GetHwnd();
    if (overlay)
        ScreenToClient(overlay, &p);

    return ImVec2((float)p.x, (float)p.y);
}

bool IsRobloxFocused()
{
    HWND fg = GetForegroundWindow();
    if (!fg)
        return false;

    HWND game = Renderer::GetGameHwnd();
    if (game && IsWindow(game))
    {
        if (fg == game || IsChild(game, fg))
            return true;
    }

    HWND overlay = Renderer::GetHwnd();
    if (overlay && IsWindow(overlay))
    {
        if (fg == overlay || IsChild(overlay, fg))
            return true;
    }

    return false;
}

// пустой курсор, системный иначе мигает
HCURSOR EnsureBlankCursor()
{
    if (g_blank_cursor)
        return g_blank_cursor;

    // 32x32 blank
    int k = 32;
    int and_bytes = (k * k) / 8;
    BYTE and_mask[128]; // 32*32/8
    BYTE xor_mask[128];
    memset(and_mask, 0xFF, and_bytes);
    memset(xor_mask, 0x00, and_bytes);
    g_blank_cursor = CreateCursor(GetModuleHandleW(nullptr), 0, 0, k, k, and_mask, xor_mask);
    return g_blank_cursor;
}

void UnpatchGameClassCursor()
{
    if (!g_class_cursor_patched)
        return;
    if (g_patched_hwnd && IsWindow(g_patched_hwnd))
        SetClassLongPtrW(g_patched_hwnd, GCLP_HCURSOR, (LONG_PTR)g_saved_class_cursor);
    g_class_cursor_patched = false;
    g_patched_hwnd = nullptr;
    g_saved_class_cursor = nullptr;
}

void PatchGameClassCursor(HWND game)
{
    HCURSOR blank = EnsureBlankCursor();
    if (!game || !IsWindow(game) || !blank)
        return;

    if (g_class_cursor_patched && g_patched_hwnd != game)
        UnpatchGameClassCursor();

    if (!g_class_cursor_patched)
    {
        g_saved_class_cursor = (HCURSOR)GetClassLongPtrW(game, GCLP_HCURSOR);
        g_patched_hwnd = game;
        g_class_cursor_patched = true;
    }

    SetClassLongPtrW(game, GCLP_HCURSOR, (LONG_PTR)blank);
}

void DetachInput()
{
    if (!g_attached_tid)
        return;
    AttachThreadInput(GetCurrentThreadId(), g_attached_tid, FALSE);
    g_attached_tid = 0;
}

void EnsureAttached(HWND game)
{
    if (!game || !IsWindow(game))
        return;

    DWORD pid = 0;
    const DWORD tid = GetWindowThreadProcessId(game, &pid);
    if (!tid)
        return;

    if (g_attached_tid == tid)
        return;

    DetachInput();
    if (AttachThreadInput(GetCurrentThreadId(), tid, TRUE))
        g_attached_tid = tid;
}

void RestoreOsCursor()
{
    DetachInput();
    UnpatchGameClassCursor();

    HWND overlay = Renderer::GetHwnd();
    if (overlay && IsWindow(overlay))
        SetClassLongPtrW(overlay, GCLP_HCURSOR, (LONG_PTR)LoadCursorW(nullptr, IDC_ARROW));

    SystemParametersInfoW(SPI_SETCURSORS, 0, nullptr, 0);
    while (ShowCursor(TRUE) < 0) {}
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
}

void ForceHideOsCursor(HCURSOR blank)
{
    // счётчик в минус держим, роблокс его поднимает
    for (int i = 0; i < 8; ++i)
    {
        if (ShowCursor(FALSE) < 0)
            break;
    }

    if (blank)
    {
        for (int i = 0; i < 16; ++i)
            SetCursor(blank);
    }
}

// глушим виндовый курсор пока свой рисуем
DWORD WINAPI CursorSuppressThread(LPVOID)
{
    timeBeginPeriod(1);

    bool was_on = false;
    while (!g_thread_stop.load(std::memory_order_acquire))
    {
        if (!g_want_suppress.load(std::memory_order_acquire))
        {
            if (was_on)
            {
                RestoreOsCursor();
                was_on = false;
            }

            Sleep(16);
            continue;
        }

        HCURSOR blank = EnsureBlankCursor();
        HWND game = Renderer::GetGameHwnd();
        if (game && IsWindow(game))
        {
            PatchGameClassCursor(game);
            EnsureAttached(game);
        }

        // оверлей тоже иногда мигает при click-through
        HWND overlay = Renderer::GetHwnd();
        if (overlay && IsWindow(overlay) && blank)
            SetClassLongPtrW(overlay, GCLP_HCURSOR, (LONG_PTR)blank);

        // два раза, роблокс между кадрами курсор возвращает
        ForceHideOsCursor(blank);
        ForceHideOsCursor(blank);

        was_on = true;
        Sleep(1);
    }

    RestoreOsCursor();
    timeEndPeriod(1);
    return 0;
}

void EnsureSuppressThread()
{
    if (g_thread)
        return;
    g_thread_stop.store(false, std::memory_order_release);
    g_thread = CreateThread(nullptr, 0, &CursorSuppressThread, nullptr, 0, nullptr);
    if (g_thread)
        SetThreadPriority(g_thread, THREAD_PRIORITY_TIME_CRITICAL);
}

void StopSuppressThread()
{
    g_want_suppress.store(false, std::memory_order_release);
    if (!g_thread)
        return;

    g_thread_stop.store(true, std::memory_order_release);
    WaitForSingleObject(g_thread, 1000);
    CloseHandle(g_thread);
    g_thread = nullptr;
}

void AtExitRestore()
{
    g_want_suppress.store(false, std::memory_order_release);
    g_thread_stop.store(true, std::memory_order_release);
    UnpatchGameClassCursor();
    SystemParametersInfoW(SPI_SETCURSORS, 0, nullptr, 0);
}

void EnsureAtExit()
{
    if (g_atexit_registered)
        return;
    g_atexit_registered = true;
    std::atexit(&AtExitRestore);
}

} // namespace

void NotifyInactive()
{
    g_want_suppress.store(false, std::memory_order_release);
}

// свой прицел поверх
void Render()
{
    EnsureAtExit();

    auto& cfg = g_Settings.crosshair;
    const bool active = cfg.enabled && IsRobloxFocused();
    g_want_suppress.store(active, std::memory_order_release);

    if (!cfg.enabled)
        return;

    if (active)
        EnsureSuppressThread();

    if (!active)
        return;

    ImGui::GetIO().MouseDrawCursor = false;
    ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    // сразу прячем, не ждём тик треда
    ForceHideOsCursor(EnsureBlankCursor());

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (!dl)
        return;

    ImVec2 m = CursorClient();
    float cx = std::floor(m.x) + 0.5f;
    float cy = std::floor(m.y) + 0.5f;

    float gap = cfg.gap;
    float len = cfg.length;
    float thick = cfg.thickness;
    if (gap < 0.f) gap = 0.f;
    if (len < 1.f) len = 1.f;
    if (thick < 1.f) thick = 1.f;

    ImU32 col = Col4(cfg.color);
    ImU32 outline = Col4(cfg.outline_color);
    bool out = cfg.outline;

    float angle_deg = 0.f;
    if (cfg.spin && cfg.spin_speed != 0.f)
    {
        angle_deg = std::fmod((float)ImGui::GetTime() * cfg.spin_speed, 360.f);
        if (angle_deg < 0.f)
            angle_deg += 360.f;
    }

    // 3.14159...
    float pi = 3.14159265f;
    float base = angle_deg * (pi / 180.f) - (pi * 0.5f);
    ImDrawListFlags backup = dl->Flags;
    dl->Flags |= ImDrawListFlags_AntiAliasedLines;

    for (int i = 0; i < 4; ++i)
    {
        float a = base + (float)i * (pi * 0.5f);
        float c = std::cos(a);
        float s = std::sin(a);
        ImVec2 p0(cx + c * gap, cy + s * gap);
        ImVec2 p1(cx + c * (gap + len), cy + s * (gap + len));
        if (out)
            dl->AddLine(p0, p1, outline, thick + 2.f);
        dl->AddLine(p0, p1, col, thick);
    }

    if (cfg.dot)
    {
        float r = cfg.dot_size;
        if (r < 1.f) r = 1.f;
        if (out)
            dl->AddCircleFilled(ImVec2(cx, cy), r + 1.f, outline, 16);
        dl->AddCircleFilled(ImVec2(cx, cy), r, col, 16);
    }

    dl->Flags = backup;
}

void Shutdown()
{
    StopSuppressThread();
    RestoreOsCursor();

    if (g_blank_cursor)
    {
        DestroyCursor(g_blank_cursor);
        g_blank_cursor = nullptr;
    }
}

} // namespace Crosshair
} // namespace Visuals
} // namespace Cheat
