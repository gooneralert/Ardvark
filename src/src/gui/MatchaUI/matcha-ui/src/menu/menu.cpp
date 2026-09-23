#include "menu.h"
#include <atomic>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/backends/imgui_impl_dx11.h>
#include <imgui/backends/imgui_impl_win32.h>
#include <imgui/misc/imgui_freetype.h>
#include <render/textures/texture.h>
#include "../../../ext/imgui/addons/imgui_addons.h"
#include <settings.h>
#include <stubs.h>
#include <menu/keybind/keybind.h>
#include <shlobj.h>
#include <render/render.h>
#include <render/backdrop_blur.h>
#include <render/3d/main_api.hpp>
#include <render/3d/texture/texture_cache.hpp>
#include "../../ext/font/inter.h"
#include "../../ext/font/inter_semibold.h"
#include "menu_theme.h"
#include "avatar_preview_png.h"
#include "g_triangle_arrow.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <mutex>
#include <map>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

extern ImFont* esp_font;
extern ImFont* esp_font_tahoma;
extern ImFont* esp_font_sp7;
extern ImFont* esp_font_arial;

// Exposed from render.cpp for the debug overlay
extern RECT s_explorer_rect;
extern bool s_explorer_visible;

class AvatarManager;
extern AvatarManager* g_avatar_manager;

namespace helper
{
    void draw_text_outlined(ImDrawList* draw, ImFont* font, float font_size, ImVec2 pos, ImU32 col, const char* text_begin, const char* text_end = nullptr);
    void corner_box(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 col, float thickness = 1.f);
}

// ---------------------------------------------------------------------------
//  Sidebar-tab (Combat/Visuals/Rage/Config/Settings) switch fade — file-scope
//  state so it doesn't depend on staying in lexical scope across the huge
//  nav_page if/else-if content block below. Call Update() once near the top
//  of that block, then GetEased() right before drawing the reveal overlay.
// ---------------------------------------------------------------------------
namespace
{
    // Main tabs (Combat/Visuals/Rage/Config/Settings): fade out then staggered slide down
    namespace NavAnim
    {
        static int   s_display     = 0;
        static int   s_target      = 0;
        static int   s_row         = 0;
        static float s_fade_out_t  = 0.f;
        static float s_slide_t     = 999.f;
        static bool  s_fading_out  = false;
        static bool  s_initialized = false;

        static float Smooth(float t)
        {
            t = ImClamp(t, 0.f, 1.f);
            return t * t * (3.f - 2.f * t);
        }

        void EnsureInit(int page)
        {
            if (!s_initialized)
            {
                s_display = page;
                s_target  = page;
                s_initialized = true;
            }
        }

        void Request(int page)
        {
            if (page == s_target && !s_fading_out)
                return;
            s_target = page;
            if (!s_fading_out)
            {
                s_fading_out = true;
                s_fade_out_t = 0.f;
            }
        }

        void Update(float dt)
        {
            constexpr float kFadeOut = 0.11f;
            constexpr float kSlideWindow = 0.60f;
            dt = ImClamp(dt, 0.f, 0.05f);
            if (s_fading_out)
            {
                s_fade_out_t += dt / kFadeOut;
                if (s_fade_out_t >= 1.f)
                {
                    s_fade_out_t = 1.f;
                    s_fading_out = false;
                    s_display    = s_target;
                    s_slide_t    = 0.f;
                }
            }
            else if (s_slide_t < kSlideWindow)
                s_slide_t += dt;
        }

        int  Display() { return s_display; }
        int  Target()  { return s_target; }

        float PanelAlpha()
        {
            if (s_fading_out)
                return 1.f - Smooth(s_fade_out_t);
            return 1.f;
        }

        float RowProgress(int row)
        {
            if (s_fading_out)
                return 1.f;
            constexpr float kSlideWindow = 0.60f;
            if (s_slide_t >= kSlideWindow)
                return 1.f;
            constexpr float kRowDelay = 0.020f;
            constexpr float kRowDur   = 0.15f;
            const float t = s_slide_t - row * kRowDelay;
            if (t <= 0.f)
                return 0.f;
            return Smooth(ImClamp(t / kRowDur, 0.f, 1.f));
        }

        void BeginPanel()
        {
            s_row = 0;
            ImAdd::SetStaggerAnim(PanelAlpha(), RowProgress);
        }

        int NextRow() { return s_row++; }

        struct RowScope
        {
            const int  idx;
            const bool active;
            RowScope() : idx(NextRow()), active(ImAdd::BeginStaggerRow(idx)) {}
            ~RowScope() { if (active) ImAdd::EndStaggerRow(); }
            explicit operator bool() const { return active; }
        };
    }

    #define NAV_ROW if (NavAnim::RowScope _nav_row; _nav_row)

    // Aim-type tab (Aimbot / Silent Aim / Colorbot / Triggerbot) transition:
    // fade the outgoing panel out, swap content, then stagger rows in.
    namespace AimTabAnim
    {
        static int   s_display     = 0;
        static int   s_target      = 0;
        static int   s_row         = 0;
        static float s_fade_out_t  = 0.f;
        static float s_slide_t     = 999.f;
        static bool  s_fading_out  = false;
        static bool  s_initialized = false;

        static float Smooth(float t)
        {
            t = ImClamp(t, 0.f, 1.f);
            return t * t * (3.f - 2.f * t);
        }

        void EnsureInit(int tab)
        {
            if (!s_initialized)
            {
                s_display     = tab;
                s_target      = tab;
                s_initialized = true;
            }
        }

        void RequestTab(int tab)
        {
            if (tab == s_target && !s_fading_out)
                return;
            s_target = tab;
            if (!s_fading_out)
            {
                s_fading_out = true;
                s_fade_out_t = 0.f;
            }
        }

        void Update(float dt)
        {
            constexpr float kFadeOut = 0.11f;      // quick fade-out
            constexpr float kSlideWindow = 0.55f;  // total stagger window

            dt = ImClamp(dt, 0.f, 0.05f); // clamp huge dt on lag spikes
            if (s_fading_out)
            {
                s_fade_out_t += dt / kFadeOut;
                if (s_fade_out_t >= 1.f)
                {
                    s_fade_out_t = 1.f;
                    s_fading_out = false;
                    s_display    = s_target;
                    s_slide_t    = 0.f;
                }
            }
            else if (s_slide_t < kSlideWindow)
                s_slide_t += dt;
        }

        int  DisplayTab() { return s_display; }
        int  TargetTab()  { return s_target; }

        float PanelAlpha()
        {
            if (s_fading_out)
                return 1.f - Smooth(s_fade_out_t);
            return 1.f;
        }

        float RowProgress(int row)
        {
            if (s_fading_out)
                return 1.f;

            constexpr float kSlideWindow = 0.55f;
            if (s_slide_t >= kSlideWindow)
                return 1.f;

            // Fast stagger: short delay, slightly longer duration for smoothness
            constexpr float kRowDelay = 0.022f;
            constexpr float kRowDur   = 0.16f;
            const float t = s_slide_t - row * kRowDelay;
            if (t <= 0.f)
                return 0.f;
            return Smooth(ImClamp(t / kRowDur, 0.f, 1.f));
        }

        void BeginPanel()
        {
            s_row = 0;
            ImAdd::SetStaggerAnim(PanelAlpha(), RowProgress);
        }

        int NextRow() { return s_row++; }

        struct RowScope
        {
            const int  idx;
            const bool active;
            RowScope() : idx(NextRow()), active(ImAdd::BeginStaggerRow(idx)) {}
            ~RowScope() { if (active) ImAdd::EndStaggerRow(); }
            explicit operator bool() const { return active; }
        };

        void TriggerEntrance()
        {
            if (!s_fading_out)
                s_slide_t = 0.f;
        }
    }

    namespace OpenCloseAnim
    {
        float Ease()
        {
            const float t = ImClamp(Menu::m_fMenuAlpha, 0.f, 1.f);
            return t * t * (3.f - 2.f * t);
        }

        void Tick()
        {
            const float dt = ImGui::GetIO().DeltaTime;
            const float dur = Menu::m_bClosing ? 0.18f : 0.26f;
            const float target = Menu::m_bClosing ? 0.f : 1.f;
            if (Menu::m_fMenuAlpha < target)
                Menu::m_fMenuAlpha = ImMin(target, Menu::m_fMenuAlpha + dt / dur);
            else if (Menu::m_fMenuAlpha > target)
                Menu::m_fMenuAlpha = ImMax(target, Menu::m_fMenuAlpha - dt / dur);
            if (Menu::m_bClosing && Menu::m_fMenuAlpha < 0.002f)
                Menu::m_fMenuAlpha = 0.f;
            Menu::m_fPreviewAlpha = Menu::m_fMenuAlpha;
        }

        void ScaleDrawList(ImDrawList* dl, ImVec2 origin, float scale, float y_shift)
        {
            if (!dl)
                return;
            for (int i = 0; i < dl->VtxBuffer.Size; ++i)
            {
                dl->VtxBuffer[i].pos.x = origin.x + (dl->VtxBuffer[i].pos.x - origin.x) * scale;
                dl->VtxBuffer[i].pos.y = origin.y + (dl->VtxBuffer[i].pos.y - origin.y) * scale + y_shift;
            }
            for (int i = 0; i < dl->CmdBuffer.Size; ++i)
            {
                ImVec4& cr = dl->CmdBuffer[i].ClipRect;
                const float x0 = origin.x + (cr.x - origin.x) * scale;
                const float y0 = origin.y + (cr.y - origin.y) * scale + y_shift;
                const float x1 = origin.x + (cr.z - origin.x) * scale;
                const float y1 = origin.y + (cr.w - origin.y) * scale + y_shift;
                cr.x = x0; cr.y = y0; cr.z = x1; cr.w = y1;
            }
        }

        bool IsMenuRoot(ImGuiWindow* w)
        {
            if (!w || !w->Name)
                return false;
            const char* n = w->Name;
            return strcmp(n, "##MatchaMain") == 0
                || strcmp(n, "##QuannwarePreview") == 0
                || strcmp(n, "##ThemePanel") == 0
                || strcmp(n, "##ExplorerBottomPanel") == 0
                || strcmp(n, "##PlayersWindow") == 0;
        }

        void ApplyZoom()
        {
            const float eased = Ease();
            const float scale = ImLerp(0.92f, 1.f, eased);
            const float y_shift = (1.f - eased) * 16.f;
            if (scale >= 0.9995f && y_shift < 0.05f)
                return;

            ImGuiWindow* main = ImGui::FindWindowByName("##MatchaMain");
            const ImVec2 origin = main
                ? ImVec2(main->Pos.x + main->Size.x * 0.5f, main->Pos.y + main->Size.y * 0.5f)
                : ImGui::GetIO().DisplaySize * 0.5f;

            ImGuiContext& g = *GImGui;
            for (int i = 0; i < g.Windows.Size; ++i)
            {
                ImGuiWindow* w = g.Windows[i];
                if (!w || !w->Active)
                    continue;
                ImGuiWindow* root = w->RootWindow ? w->RootWindow : w;
                if (!IsMenuRoot(root))
                    continue;
                ScaleDrawList(w->DrawList, origin, scale, y_shift);
            }
        }
    }

    #define AIM_ROW if (AimTabAnim::RowScope _aim_row; _aim_row)
}


#if 0
namespace spotify_now
{
    struct Track
    {
        std::string artist;
        std::string title;
        bool        playing  = false;
        std::string art_path;   // path to the newest cover-art file (may be empty)
    };
    static Track      s_track;
    static std::mutex s_mtx;
    static bool       s_started = false;

    // D3D11 texture for current cover art
    static ID3D11ShaderResourceView* s_art_srv   = nullptr;
    static std::string               s_art_loaded;  // path that s_art_srv was built from

    struct EnumCtx { std::string title; };

    static BOOL CALLBACK EnumProc(HWND hwnd, LPARAM lp)
    {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (!pid) return TRUE;

        HANDLE hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!hp) return TRUE;
        char exe[MAX_PATH] = {};
        DWORD len = MAX_PATH;
        bool is_spotify = false;
        if (QueryFullProcessImageNameA(hp, 0, exe, &len))
        {
            std::string p(exe);
            auto slash = p.find_last_of("\\/");
            std::string name = (slash != std::string::npos) ? p.substr(slash + 1) : p;
            for (auto& c : name) c = (char)tolower((unsigned char)c);
            is_spotify = (name == "spotify.exe");
        }
        CloseHandle(hp);
        if (!is_spotify) return TRUE;

        char buf[512] = {};
        int n = GetWindowTextA(hwnd, buf, (int)sizeof(buf));
        if (n <= 0) return TRUE;
        std::string t(buf, n);
        if (t.find(" - ") != std::string::npos)
        {
            reinterpret_cast<EnumCtx*>(lp)->title = t;
            return FALSE;
        }
        return TRUE;
    }

    // Find the best album-art JPEG in Spotify's Chromium cache.
    // Strategy: album art is always >= 80 KB; ads and UI elements are smaller.
    // Pick the LARGEST JPEG >= 80 KB written in the last 90 seconds.
    // If nothing that fresh exists, fall back to the largest JPEG >= 80 KB overall.
    static std::string find_latest_cover_art()
    {
        char local[MAX_PATH] = {};
        if (!GetEnvironmentVariableA("LOCALAPPDATA", local, MAX_PATH)) return {};
        std::string dir = std::string(local) + "\\Spotify\\Browser\\Cache\\Cache_Data";

        WIN32_FIND_DATAA fd;
        HANDLE hf = FindFirstFileA((dir + "\\f_*").c_str(), &fd);
        if (hf == INVALID_HANDLE_VALUE) return {};

        FILETIME now_ft;
        GetSystemTimeAsFileTime(&now_ft);
        ULONGLONG now_ull = ((ULONGLONG)now_ft.dwHighDateTime << 32) | now_ft.dwLowDateTime;
        // 90 seconds in 100-ns units
        constexpr ULONGLONG kWindow = 90ULL * 10000000ULL;
        constexpr ULONGLONG kMinSz  = 81920ULL; // 80 KB minimum â€” filters ads/UI

        ULONGLONG best_recent_sz = 0;
        std::string best_recent_path;
        ULONGLONG best_any_sz = 0;
        std::string best_any_path;

        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            ULONGLONG sz = ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            if (sz < kMinSz) continue;

            // Verify JPEG magic
            std::string fp = dir + "\\" + fd.cFileName;
            HANDLE hc = CreateFileA(fp.c_str(), GENERIC_READ, FILE_SHARE_READ,
                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hc == INVALID_HANDLE_VALUE) continue;
            unsigned char hdr[2] = {};
            DWORD rd = 0;
            ReadFile(hc, hdr, 2, &rd, nullptr);
            CloseHandle(hc);
            if (rd < 2 || hdr[0] != 0xFF || hdr[1] != 0xD8) continue;

            // Track largest within window
            ULONGLONG file_ull = ((ULONGLONG)fd.ftLastWriteTime.dwHighDateTime << 32)
                               | fd.ftLastWriteTime.dwLowDateTime;
            if (now_ull >= file_ull && (now_ull - file_ull) <= kWindow)
            {
                if (sz > best_recent_sz) { best_recent_sz = sz; best_recent_path = fp; }
            }
            // Track largest overall as fallback
            if (sz > best_any_sz) { best_any_sz = sz; best_any_path = fp; }

        } while (FindNextFileA(hf, &fd));
        FindClose(hf);

        return !best_recent_path.empty() ? best_recent_path : best_any_path;
    }
    static void poll_thread()
    {
        while (true) {
            EnumCtx ctx;
            EnumWindows(EnumProc, reinterpret_cast<LPARAM>(&ctx));
            Track t;
            if (!ctx.title.empty())
            {
                t.playing = true;
                auto sep = ctx.title.find(" - ");
                if (sep != std::string::npos)
                {
                    t.artist = ctx.title.substr(0, sep);
                    t.title  = ctx.title.substr(sep + 3);
                }
                else
                {
                    t.title = ctx.title;
                }
                t.art_path = find_latest_cover_art();
            }
            { std::lock_guard<std::mutex> lk(s_mtx); s_track = t; }
            Sleep(500);
        }
    }

    static void ensure_started()
    {
        if (s_started) return;
        s_started = true;
        std::thread(poll_thread).detach();
    }

    static Track get() { std::lock_guard<std::mutex> lk(s_mtx); return s_track; }

    // Call from the render thread: upload art texture if the path changed.
    static ID3D11ShaderResourceView* get_art_srv(const std::string& path)
    {
        if (path.empty()) return nullptr;
        if (path == s_art_loaded) return s_art_srv;
        // Load the file into memory then decode with D3D11CreateTextureWithMips
        HANDLE hf = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hf == INVALID_HANDLE_VALUE) return nullptr;
        DWORD sz = GetFileSize(hf, nullptr);
        if (sz == 0 || sz > 8 * 1024 * 1024) { CloseHandle(hf); return nullptr; }
        std::vector<unsigned char> buf(sz);
        DWORD rd = 0;
        ReadFile(hf, buf.data(), sz, &rd, nullptr);
        CloseHandle(hf);
        if (rd != sz) return nullptr;

        if (!render || !render->detail ||
            !render->detail->device || !render->detail->device_context) return nullptr;

        if (s_art_srv) { s_art_srv->Release(); s_art_srv = nullptr; }
        s_art_srv = D3D11CreateTextureWithMips(
            render->detail->device, render->detail->device_context,
            buf.data(), (size_t)sz);
        s_art_loaded = s_art_srv ? path : "";
        return s_art_srv;
    }
}
#endif

static void EnsureUiIconsLoaded()
{
    if (MenuTheme::combo_arrow_srv || !render || !render->detail ||
        !render->detail->device || !render->detail->device_context) return;
    MenuTheme::combo_arrow_srv = D3D11CreateTextureScaled(
        render->detail->device, render->detail->device_context,
        g_triangle_arrow_png, g_triangle_arrow_png_len, 10, 10, true);
}

#define PROJECT_NAME    external_config::cheat_name.c_str()

bool Menu::Initialize(HWND hWnd, ID3D11Device* pDevice, ID3D11DeviceContext* pDeviceContext)
{
    bool result = true;

    IMGUI_CHECKVERSION();
    if (!ImGui::GetCurrentContext())
    {
        ImGui::CreateContext();
    }

    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();
    MenuTheme::Apply(io);

    // Inter Regular + SemiBold. Light hinting (ClearType-style) + no bitmap
    // strikes. FontGlobalScale is left at 1; layout scale uses FontScaleMain
    // so 1.92 re-rasters at the real pixel size instead of blurring.
    auto make_inter_cfg = [](float multiply) {
        ImFontConfig cfg;
        cfg.FontDataOwnedByAtlas = false;
        cfg.PixelSnapH           = true;
        cfg.OversampleH          = 2;
        cfg.OversampleV          = 2;
        cfg.RasterizerMultiply   = multiply;
        cfg.FontLoaderFlags      = ImGuiFreeTypeLoaderFlags_LightHinting |
                                   ImGuiFreeTypeLoaderFlags_ForceAutoHint;
        return cfg;
    };

    ImFontConfig reg_cfg = make_inter_cfg(1.20f);
    ImFont* inter_reg = io.Fonts->AddFontFromMemoryTTF(
        (void*)font_inter, (int)sizeof(font_inter), 15.0f, &reg_cfg);

    ImFontConfig sb_cfg = make_inter_cfg(1.15f);
    ImFont* inter_sb = io.Fonts->AddFontFromMemoryTTF(
        (void*)inter_semibold_hex, (int)sizeof(inter_semibold_hex), 15.0f, &sb_cfg);

    if (!inter_reg)
    {
        ImFontConfig def_cfg;
        inter_reg = io.Fonts->AddFontDefault(&def_cfg);
    }
    if (!inter_sb)
        inter_sb = inter_reg;

    MenuTheme::Fonts::Regular  = inter_reg;
    MenuTheme::Fonts::SemiBold = inter_sb;
    MenuTheme::Fonts::Bold     = inter_sb;
    io.FontDefault             = inter_reg;

    result = ImGui_ImplWin32_Init(hWnd);
    if (!result) return false;

    result = ImGui_ImplDX11_Init(pDevice, pDeviceContext);
    if (!result) return false;

    m_bInitialized = true;
    return true;
}

void Menu::Render()
{
    if (!m_bInitialized) return;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    {
        DrawMenu();
    }
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

// Forward-declare the startup namespace so DrawLoadingScreen can read it.
// (Defined in main.cpp — only used after the render loop is running.)
namespace startup {
    extern std::atomic<const char*> status_msg;
    extern std::atomic<float>       progress;
    extern std::atomic<bool>        ready;
}

#if 0
void Menu::DrawLauncher()
{
    // -----------------------------------------------------------------------
    // Launcher — interactable page BEFORE fake loading screen
    // Same layout/colors as main menu (MenuTheme), not Matcha pink.
    // Shows Kernel/Usermode (Usermode Recommended) + Resolution + Launch
    // -----------------------------------------------------------------------
    // UIScale math — solves hardcoded 2560x1440 fitting all resolutions.
    // Manual override (user typed non-default) -> width/2560, else auto min(displayW/2560, displayH/1440)
    // This makes menu/launcher look identical % on any res.
    float UIScale = 1.0f;
    {
        if (settings::menu::screen_width != 2560 || settings::menu::screen_height != 1440)
        {
            UIScale = (settings::menu::screen_width <= 0) ? 1.0f : static_cast<float>(settings::menu::screen_width) / 2560.0f;
        }
        else
        {
            ImVec2 d = ImGui::GetIO().DisplaySize;
            if (d.x < 100.f || d.y < 100.f) { d.x = static_cast<float>(GetSystemMetrics(SM_CXSCREEN)); d.y = static_cast<float>(GetSystemMetrics(SM_CYSCREEN)); }
            float sx = d.x / 2560.0f, sy = d.y / 1440.0f;
            UIScale = (sx < sy) ? sx : sy;
            if (UIScale <= 0.f) UIScale = 1.0f;
            UIScale = ImClamp(UIScale, 0.50f, 1.0f);
        }
        ::UIScale = UIScale;
        ::settings::UIScale = UIScale;
        settings::menu::menu_uiscale = UIScale;
        // keep settings::menu::UpdateUIScale consistent but don't overwrite auto
        // (Update would use width-only; we already handled)
    }

    // BIGGER FIXED SIZE — fills previous empty space, same on every monitor
    constexpr float kLauncherW_base = 540.f;
    constexpr float kLauncherH_base = 520.f; // was 720, too much empty room
    const float W = kLauncherW_base; // fixed
    const float H = kLauncherH_base;
    const float kRound = 16.f;

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const ImVec2 pp = ImVec2(ImFloor(display.x * 0.5f - W * 0.5f),
                             ImFloor(display.y * 0.5f - H * 0.5f));
    const ImVec2 pm = ImVec2(pp.x + W, pp.y + H);
    const float pad = 16.f;

    // Sync launcher mode with settings
    // Menu::m_iLauncherMode <-> settings::launcher::attach_method
    if (settings::launcher::attach_method != m_iLauncherMode)
        settings::launcher::attach_method = m_iLauncherMode;
    if (m_iLauncherMode != settings::launcher::attach_method)
        m_iLauncherMode = settings::launcher::attach_method;

    // Fullscreen dim (same as loading screen)
    ImDrawList* bg_dl = ImGui::GetBackgroundDrawList();
    bg_dl->AddRectFilled(ImVec2(0,0), display, IM_COL32(0,0,0,180));

    ImGui::SetNextWindowPos(pp);
    ImGui::SetNextWindowSize(ImVec2(W, H));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::Begin("##MatchaLauncher", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Panel bg/border — MATCH MENU: black, barely see-through
    MenuTheme::DrawWindowGlow(pp, pm, kRound);
    backdrop_blur::fill(dl, pp, pm, kRound, IM_COL32(37, 37, 46, 250));
    dl->AddRect(pp, pm, IM_COL32(255, 255, 255, 16), kRound, 0, 1.f);

    // Titlebar separator (thin line under top bar) — like MenuTheme
    const float topBarH = 36.f;
    // Window controls — top-right X and — (minimize)
    {
        constexpr float kBtnSz = 22.f;
        const float btnPad = 10.f;
        const ImVec2 xMax{ pm.x - btnPad, pp.y + topBarH*0.5f + kBtnSz*0.5f };
        const ImVec2 xMin{ xMax.x - kBtnSz, xMax.y - kBtnSz };
        const ImVec2 mMax{ xMin.x - 6.f, xMax.y };
        const ImVec2 mMin{ mMax.x - kBtnSz, mMax.y - kBtnSz };

        POINT cur{}; GetCursorPos(&cur);
        auto over = [&](ImVec2 a, ImVec2 b){ return cur.x >= (int)a.x && cur.x <= (int)b.x && cur.y >= (int)a.y && cur.y <= (int)b.y; };
        bool hovX = over(xMin, xMax);
        bool hovM = over(mMin, mMax);
        bool lDn = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        static bool s_prevDn = false;
        bool clickedX = hovX && lDn && !s_prevDn;
        bool clickedM = hovM && lDn && !s_prevDn;
        s_prevDn = lDn;

        // Hover bg
        if (hovX) dl->AddRectFilled(xMin, xMax, IM_COL32(180,45,45,160), 4.f);
        else dl->AddRectFilled(xMin, xMax, IM_COL32(30,30,36,120), 4.f);
        if (hovM) dl->AddRectFilled(mMin, mMax, IM_COL32(50,50,60,140), 4.f);
        else dl->AddRectFilled(mMin, mMax, IM_COL32(30,30,36,100), 4.f);

        // X lines
        {
            ImU32 xc = IM_COL32(220,220,220,230);
            float p = 6.f;
            dl->AddLine(ImVec2(xMin.x+p, xMin.y+p), ImVec2(xMax.x-p, xMax.y-p), xc, 1.5f);
            dl->AddLine(ImVec2(xMax.x-p, xMin.y+p), ImVec2(xMin.x+p, xMax.y-p), xc, 1.5f);
        }
        // — line
        dl->AddLine(ImVec2(mMin.x+5.f, mMin.y + kBtnSz*0.5f), ImVec2(mMax.x-5.f, mMax.y - kBtnSz*0.5f), IM_COL32(200,200,210,200), 1.5f);

        if (clickedX) ExitProcess(0);
        if (clickedM) ShowWindow(render->detail->window, SW_MINIMIZE);

        // Hit-test helper for wnd_proc — the launcher window itself will be found via FindWindowByName,
        // but we also keep the X/min interactive via raw polling.
    }

    // Header — brand mark inline with Launcher
    {
        static ID3D11ShaderResourceView* s_brandLauncherSrv = nullptr;
        const float logoSide = 22.f;
        ImVec2 logoPos(pp.x + pad, pp.y + (topBarH - logoSide) * 0.5f);
        if (s_brandLauncherSrv)
            dl->AddImage((ImTextureID)s_brandLauncherSrv, logoPos, ImVec2(logoPos.x + logoSide, logoPos.y + logoSide));
        else {
            ImFont* fTmp = ImGui::GetFont();
            dl->AddText(fTmp, 13.f, logoPos, IM_COL32(255,255,255,255), "M");
        }
        ImFont* fSmall = ImGui::GetFont();
        float fSmallSz = 13.f;
        ImU32 colLaunch = IM_COL32(255,255,255,255);
        float x = logoPos.x + logoSide + 8.f;
        float y = pp.y + (topBarH - fSmallSz) * 0.5f;
        dl->AddText(fSmall, fSmallSz, ImVec2(x, y), colLaunch, "Loader");
        float sepY = pp.y + topBarH;
        dl->AddLine(ImVec2(pp.x + pad, sepY), ImVec2(pm.x - pad, sepY), IM_COL32(255,255,255,14), 1.f);
    }

    // Content start
    float curY = pp.y + topBarH + 16.f;
    const float contentW = W - pad*2.f;

    // "Select Mode" centered (was left)
    {
        ImFont* fTitle = MenuTheme::Fonts::SemiBold ? MenuTheme::Fonts::SemiBold : ImGui::GetFont();
        float tSz = 22.f;
        const char* title = "Select Mode";
        ImVec2 tDim = fTitle->CalcTextSizeA(tSz, FLT_MAX, 0.f, title);
        dl->AddText(fTitle, tSz, ImVec2(pp.x + (W - tDim.x)*0.5f, curY), IM_COL32(255,255,255,255), title);
        curY += tDim.y + 4.f;
        ImFont* fSub = ImGui::GetFont();
        float sSz = 12.f;
        const char* sub = "Choose your attach method";
        ImVec2 sDim = fSub->CalcTextSizeA(sSz, FLT_MAX, 0.f, sub);
        dl->AddText(fSub, sSz, ImVec2(pp.x + (W - sDim.x)*0.5f, curY), IM_COL32(140,140,150,220), sub);
        curY += sDim.y + 8.f;
    }

    // Mode rows — bigger to fill open space (was 46)
    {
        const float rowH = 58.f;
        const float rowGap = 10.f;
        const float rowR = 8.f;
        const char* labels[2] = { "Kernel", "Usermode" };
        for (int i = 0; i < 2; ++i)
        {
            ImVec2 rMin(pp.x + pad, curY);
            ImVec2 rMax(rMin.x + contentW, rMin.y + rowH);
            bool selected = (m_iLauncherMode == i);
            // InvisibleButton for selection
            ImGui::SetCursorScreenPos(rMin);
            ImGui::PushID(i);
            char id[16]; snprintf(id, sizeof(id), "##mode_%d", i);
            bool pressed = ImGui::InvisibleButton(id, ImVec2(contentW, rowH));
            ImGui::PopID();
            if (pressed)
            {
                m_iLauncherMode = i;
                settings::launcher::attach_method = i;
            }
            bool hovered = ImGui::IsItemHovered();
            // Hover animation — smooth lerp 0→1
            static float s_hovAnim[2] = {0,0};
            float dt = ImGui::GetIO().DeltaTime;
            s_hovAnim[i] = s_hovAnim[i] + ( (hovered?1.f:0.f) - s_hovAnim[i]) * ImClamp(dt*10.f,0.f,1.f);
            // ALL BLACK 85% (0,0,0,217) — background 75% (191), buttons more opaque
            ImU32 bg = IM_COL32(0,0,0,217);
            // border animates white 18% → 60% on hover, 230 when selected
            float bordA = selected ? 230.f : (18.f + 42.f * s_hovAnim[i]);
            ImU32 border = IM_COL32(255,255,255,(int)bordA);
            // subtle hover fill overlay (very dark grey at 85% still black, add faint white overlay)
            // keep bg black, hover is border only — no grey
            dl->AddRectFilled(rMin, rMax, bg, rowR);
            dl->AddRect(rMin, rMax, border, rowR, 0, selected ? 1.4f : 1.f);
            // Label left
            ImFont* fr = ImGui::GetFont();
            float fSz = 14.f;
            dl->AddText(fr, fSz, ImVec2(rMin.x + 14.f, rMin.y + (rowH - fSz)*0.5f),
                        selected ? IM_COL32(255,255,255,255) : IM_COL32(200,200,210,255), labels[i]);
            // Recommended badge on Usermode — white, not pink
            if (i == 1)
            {
                const char* badge = "Recommended";
                float bSz = 11.f;
                ImVec2 bDim = fr->CalcTextSizeA(bSz, FLT_MAX, 0.f, badge);
                ImU32 badgeCol = IM_COL32(255,255,255,255);
                dl->AddText(fr, bSz, ImVec2(rMax.x - bDim.x - 14.f, rMin.y + (rowH - bSz)*0.5f), badgeCol, badge);
            }
            curY += rowH + rowGap;
        }
        curY += 6.f;
    }

    // Separator line
    dl->AddLine(ImVec2(pp.x + pad, curY), ImVec2(pm.x - pad, curY), IM_COL32(255,255,255,12), 1.f);
    curY += 12.f;

    // Resolution — custom W/H + Apply on same line, dropdown below (all black, white text)
    {
        ImFont* fSmall = ImGui::GetFont();
        float labSz = 11.f;
        dl->AddText(fSmall, labSz, ImVec2(pp.x + pad, curY), IM_COL32(160,160,170,220), "Resolution");
        {
            char sub[64];
            snprintf(sub, sizeof(sub), "Base 2560x1440  |  UIScale %.2f", settings::menu::GetMenuUIScale());
            ImVec2 sd = fSmall->CalcTextSizeA(labSz, FLT_MAX, 0.f, sub);
            dl->AddText(fSmall, labSz, ImVec2(pm.x - pad - sd.x, curY), IM_COL32(130,130,140,180), sub);
        }
        curY += 16.f;

        static int s_laEditW = 0, s_laEditH = 0;
        static bool s_laInit = false;
        if (!s_laInit) { s_laEditW = settings::menu::screen_width; s_laEditH = settings::menu::screen_height; s_laInit = true; }

        const float gap = 8.f;
        const float applyW = 84.f;
        const float colW = (contentW - gap*2.f - applyW) * 0.5f; // smaller to fit Apply

        dl->AddText(fSmall, 10.f, ImVec2(pp.x + pad, curY), IM_COL32(130,130,140,200), "Screen Width");
        dl->AddText(fSmall, 10.f, ImVec2(pp.x + pad + colW + gap, curY), IM_COL32(130,130,140,200), "Screen Height");
        curY += 12.f;

        // Row: W | H | Apply (Apply black like Launch)
        ImGui::SetCursorScreenPos(ImVec2(pp.x + pad, curY));
        ImGui::PushItemWidth(colW);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.f, 4.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f,0.0f,0.0f,0.85f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.0f,0.0f,0.0f,0.85f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.09f,0.09f,0.11f,1.f));
        ImGui::InputInt("##LauncherW", &s_laEditW, 0, 0);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Type e.g. 1920");
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
        ImGui::PopItemWidth();

        ImGui::SameLine(0, gap);
        ImGui::PushItemWidth(colW);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.f, 4.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f,0.0f,0.0f,0.85f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.0f,0.0f,0.0f,0.85f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.09f,0.09f,0.11f,1.f));
        ImGui::InputInt("##LauncherH", &s_laEditH, 0, 0);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Height stored only");
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
        ImGui::PopItemWidth();

        ImGui::SameLine(0, gap);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f,0.0f,0.0f,0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f,0.0f,0.0f,0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f,0.0f,0.0f,0.85f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,1,1));
        if (ImGui::Button("Apply##L", ImVec2(applyW, 24.f))) {
            settings::menu::screen_width = s_laEditW;
            settings::menu::screen_height = s_laEditH;
            if (settings::menu::screen_height <= 0) settings::menu::screen_height = 1440;
            settings::menu::UpdateUIScale();
        }
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();

        curY += 30.f;
        // Dropdown for common resolutions — black, white text like Launch
        {
            ImGui::SetCursorScreenPos(ImVec2(pp.x + pad, curY));
            ImGui::PushItemWidth(contentW);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.f, 4.f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f,0.0f,0.0f,0.85f));
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f,0.0f,0.0f,0.85f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.0f,0.0f,0.0f,0.85f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.0f,0.0f,0.0f,0.85f));
            ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.0f,0.0f,0.0f,0.85f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,1,1));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.22f,0.22f,0.24f,1.f));
            const char* items[] = {"Custom", "2560x1440", "1920x1080", "3840x2160", "2560x1080", "1280x720", "1600x900", "1920x1200"};
            static int curRes = 0;
            {
                struct R{int w,h;}; R pr[]={{2560,1440},{1920,1080},{3840,2160},{2560,1080},{1280,720},{1600,900},{1920,1200}};
                curRes = 0;
                for(int i=0;i<7;i++) if(s_laEditW==pr[i].w && s_laEditH==pr[i].h) {curRes=i+1;break;}
            }
            const char* presetNames[] = {"2560x1440","1920x1080","3840x2160","2560x1080","1280x720","1600x900","1920x1200"};
            const char* curLabel = curRes==0 ? "Common resolutions..." : presetNames[curRes-1];
            if (ImGui::BeginCombo("##ResCombo", curLabel)) {
                for(int i=0;i<8;i++) {
                    bool sel = curRes==i;
                    if(ImGui::Selectable(items[i], sel)) {
                        curRes=i;
                        if(i>0) {
                            struct R{int w,h;}; R pr2[]={{2560,1440},{1920,1080},{3840,2160},{2560,1080},{1280,720},{1600,900},{1920,1200}};
                            s_laEditW=pr2[i-1].w; s_laEditH=pr2[i-1].h;
                        }
                    }
                    if(sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::PopStyleColor(7);
            ImGui::PopStyleVar(2);
            ImGui::PopItemWidth();
            curY += 34.f;
        }
    }

    // Launch — sits right below dropdown (no big empty gap)
    {
        const float btnH = 52.f;
        const float btnR = 8.f;
        ImVec2 bMin(pp.x + pad, curY + 8.f);
        ImVec2 bMax(bMin.x + contentW, bMin.y + btnH); // at curY
        // InvisibleButton for click
        ImGui::SetCursorScreenPos(bMin);
        bool launchPressed = ImGui::InvisibleButton("##LauncherLaunch", ImVec2(contentW, btnH));
        bool hovered = ImGui::IsItemHovered();
        ImU32 btnFill = hovered ? IM_COL32(18,18,20,217) : IM_COL32(0,0,0,217); // BLACK
        // If accent is near-white (1,1,1), make it slightly off-white so it pops
        // Keep as is — white on dark is the menu's accent style.
        dl->AddRectFilled(bMin, bMax, btnFill, btnR);
        dl->AddRect(bMin, bMax, IM_COL32(255,255,255,18), btnR, 0, 1.f);
        ImFont* bf = MenuTheme::Fonts::SemiBold ? MenuTheme::Fonts::SemiBold : ImGui::GetFont();
        float bSz = 14.f;
        const char* bl = "Launch";
        ImVec2 bs = bf->CalcTextSizeA(bSz, FLT_MAX, 0.f, bl);
        ImU32 tcol = IM_COL32(255,255,255,255);
        dl->AddText(bf, bSz, ImVec2(ImFloor(bMin.x + (contentW - bs.x)*0.5f), ImFloor(bMin.y + (btnH - bs.y)*0.5f)), tcol, bl);
        if (launchPressed)
        {
            // Transition to loading screen — keep same UIScale, start fake loading from 0
            m_bShowLauncher = false;
            settings::launcher::show_launcher = false;
            m_bShowLoading = true;
            settings::menu::UpdateUIScale();
            m_fLoadingStartTime = static_cast<float>(ImGui::GetTime());
            // Reset join state so loading screen waits for ready then JOIN
            // startup::progress will continue from init_thread; we don't reset it.
        }
    }

    // "v1" bottom center
    {
        ImFont* fSmall = ImGui::GetFont();
        float vSz = 10.f;
        const char* v = "v1";
        ImVec2 vs = fSmall->CalcTextSizeA(vSz, FLT_MAX, 0.f, v);
        dl->AddText(fSmall, vSz, ImVec2(ImFloor(pp.x + (W - vs.x)*0.5f), pm.y - 18.f), IM_COL32(90,90,100,180), v);
    }

    ImGui::End();
}

void Menu::DrawLoadingScreen()
{
    // -----------------------------------------------------------------------
    // REDONE fake loading — smooth fade out of launcher, dim fade, brand from left
    // -----------------------------------------------------------------------
    float UIScale = 1.0f;
    {
        if (settings::menu::screen_width != 2560 || settings::menu::screen_height != 1440)
            UIScale = (settings::menu::screen_width <= 0) ? 1.0f : static_cast<float>(settings::menu::screen_width) / 2560.0f;
        else {
            ImVec2 d = ImGui::GetIO().DisplaySize;
            if (d.x < 100.f || d.y < 100.f) { d.x = static_cast<float>(GetSystemMetrics(SM_CXSCREEN)); d.y = static_cast<float>(GetSystemMetrics(SM_CYSCREEN)); }
            float sx = d.x / 2560.0f, sy = d.y / 1440.0f;
            UIScale = (sx < sy) ? sx : sy;
            if (UIScale <= 0.f) UIScale = 1.0f;
            UIScale = ImClamp(UIScale, 0.50f, 1.0f);
        }
        ::UIScale = UIScale;
        ::settings::UIScale = UIScale;
        settings::menu::menu_uiscale = UIScale;
    }

    const char* phase_label = startup::status_msg.load();
    float progress = startup::progress.load();
    bool is_ready = startup::ready.load();

    float now = static_cast<float>(ImGui::GetTime());
    if (m_fLoadingStartTime == 0.f) m_fLoadingStartTime = now;
    float elapsed = now - m_fLoadingStartTime;

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImDrawList* bg = ImGui::GetBackgroundDrawList();

    // Smooth fade for dim: 0 -> 0.75*255 over 0.45s
    float dimInDur = 0.45f;
    float dimT = ImClamp(elapsed / dimInDur, 0.f, 1.f);
    float dimSmooth = dimT * dimT * (3.f - 2.f * dimT);
    float dimAlpha = dimSmooth * 0.75f;

    // Brand wipe — compute early so ready gate can wait for it + progress
    float brandDelay = 0.08f;
    float brandDur = 0.75f;
    float fTraw = ImClamp((elapsed - brandDelay) / brandDur, 0.f, 1.f);
    float fT = fTraw * fTraw * (3.f - 2.f * fTraw);
    float fAlpha = fT;
    // Gate: wait for actual loading (progress) AND wipe done before starting fade to menu
    static float s_readyTime = -1.f;
    static float s_fadeOutStart = -1.f;
    if (is_ready && progress >= 0.99f && fT >= 0.99f && s_readyTime < 0.f) s_readyTime = now;
    float screenFade = 1.f;
    if (s_readyTime > 0.f && (now - s_readyTime) > 3.0f) {
        if (s_fadeOutStart < 0.f) s_fadeOutStart = now;
        const float fadeOutT = ImClamp((now - s_fadeOutStart) / 0.75f, 0.f, 1.f);
        const float fadeSmooth = fadeOutT * fadeOutT * (3.f - 2.f * fadeOutT);
        screenFade = 1.f - fadeSmooth;
        if (fadeOutT >= 1.f) {
            m_bShowLoading = false;
            m_bOpenMenuAfterLoading = true;
            s_readyTime = -1.f;
            s_fadeOutStart = -1.f;
            return;
        }
    }
    fAlpha *= screenFade;
    dimAlpha *= screenFade;
    bg->AddRectFilled(ImVec2(0,0), display, IM_COL32(0,0,0, (int)(dimAlpha * 255.f)));

    ID3D11ShaderResourceView* s_brandSrv = ((ID3D11ShaderResourceView*)nullptr);
    float baseLogoW = 920.f;
    float baseLogoH = 306.f; // 920*724/2172
    float logoW = baseLogoW * UIScale;
    float logoH = baseLogoH * UIScale;
    float targetX = display.x * 0.5f - logoW * 0.5f;
    float targetY = display.y * 0.5f - logoH * 0.5f - 20.f * UIScale;
    float curX = targetX;
    float curY = targetY;

    ImGui::SetNextWindowPos(ImVec2(curX, curY));
    ImGui::SetNextWindowSize(ImVec2(logoW, logoH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0,0,0,0));
    ImGui::Begin("content", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    if (s_brandSrv) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p0(curX, curY), p1(curX + logoW, curY + logoH);
        // Phase 1: grey wipe-in. Phase 2 (wipe done): glowing white fill tracks loading progress.
        float revealW = logoW * fT;
        const bool wipeDone = fT >= 0.995f;
        auto draw_progress_glow = [&](float loadedW) {
            if (loadedW <= 1.f) return;
            loadedW = ImMin(loadedW, logoW);
            const float pulse = 0.78f + 0.22f * sinf(elapsed * 3.5f);
            const float baseSpread = ImMax(2.5f, 3.f * UIScale);
            for (int ring = 5; ring >= 1; --ring) {
                const float spread = baseSpread * ring * 0.5f;
                const int alpha = (int)(fAlpha * pulse * (32.f / ring));
                if (alpha <= 0) continue;
                const ImU32 glowCol = IM_COL32(255, 255, 255, alpha);
                const ImVec2 dirs[] = {
                    { spread, 0.f }, { -spread, 0.f }, { 0.f, spread }, { 0.f, -spread },
                    { spread, spread }, { -spread, spread }, { spread, -spread }, { -spread, -spread },
                };
                dl->PushClipRect(
                    ImVec2(p0.x - spread * 2.f, p0.y - spread * 2.f),
                    ImVec2(p0.x + loadedW + spread * 2.f, p1.y + spread * 2.f),
                    true);
                for (const ImVec2& d : dirs) {
                    dl->AddImage((ImTextureID)s_brandSrv,
                        ImVec2(p0.x + d.x, p0.y + d.y), ImVec2(p1.x + d.x, p1.y + d.y),
                        ImVec2(0, 0), ImVec2(1, 1), glowCol);
                }
                dl->PopClipRect();
            }
        };
        float barProg = ImClamp(progress, 0.f, 1.f);
        float synthProg = ImClamp((elapsed - brandDelay - brandDur - 0.15f) / 2.5f, 0.f, 1.f);
        float useProg = (is_ready && barProg >= 0.99f) ? synthProg : barProg;
        float fillW = wipeDone ? (logoW * useProg) : 0.f;
        if (fillW > logoW) fillW = logoW;

        const ImU32 greyCol = IM_COL32(110, 110, 115, (int)(fAlpha * 255.f));
        if (!wipeDone) {
            dl->PushClipRect(p0, ImVec2(p0.x + revealW, p1.y), true);
            dl->AddImage((ImTextureID)s_brandSrv, p0, p1, ImVec2(0,0), ImVec2(1,1), greyCol);
            dl->PopClipRect();
        } else {
            dl->AddImage((ImTextureID)s_brandSrv, p0, p1, ImVec2(0,0), ImVec2(1,1), greyCol);
            if (fillW > 1.f) {
                draw_progress_glow(fillW);
                dl->PushClipRect(p0, ImVec2(p0.x + fillW, p1.y), true);
                dl->AddImage((ImTextureID)s_brandSrv, p0, p1, ImVec2(0,0), ImVec2(1,1), IM_COL32(255,255,255, (int)(fAlpha * 255.f)));
                dl->PopClipRect();
            }
        }
    } else {
        ImFont* f = brand_font ? brand_font : ImGui::GetFont();
        float fSz = 64.f * UIScale;
        const char* txt = "matcha";
        ImVec2 ts = f->CalcTextSizeA(fSz, FLT_MAX, 0.f, txt);
        ImVec2 tp(curX + (logoW - ts.x)*0.5f, curY + (logoH - ts.y)*0.5f);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float revealW = logoW * fT;
        const bool wipeDone2 = fT >= 0.995f;
        float barProg2 = ImClamp(progress, 0.f, 1.f);
        float synth2 = ImClamp((elapsed - brandDelay - brandDur - 0.15f) / 2.5f, 0.f, 1.f);
        float use2 = (is_ready && barProg2 >= 0.99f) ? synth2 : barProg2;
        float fillW2 = wipeDone2 ? (logoW * use2) : 0.f;
        if (fillW2 > logoW) fillW2 = logoW;
        const ImU32 greyTextCol = IM_COL32(110, 110, 115, (int)(fAlpha * 255));
        auto draw_text_glow = [&](float loadedW) {
            if (loadedW <= 1.f) return;
            loadedW = ImMin(loadedW, logoW);
            const float pulse = 0.78f + 0.22f * sinf(elapsed * 3.5f);
            const float o = ImMax(2.5f, 3.f * UIScale);
            const ImU32 glowCol = IM_COL32(255, 255, 255, (int)(fAlpha * pulse * 70.f));
            dl->PushClipRect(ImVec2(curX, curY), ImVec2(curX + loadedW, curY + logoH), true);
            for (float dx = -o * 2.f; dx <= o * 2.f; dx += o)
                for (float dy = -o * 2.f; dy <= o * 2.f; dy += o)
                    if (dx != 0.f || dy != 0.f)
                        dl->AddText(f, fSz, ImVec2(tp.x + dx, tp.y + dy), glowCol, txt);
            dl->PopClipRect();
        };
        if (!wipeDone2) {
            dl->PushClipRect(ImVec2(curX, curY), ImVec2(curX + revealW, curY + logoH), true);
            dl->AddText(f, fSz, tp, greyTextCol, txt);
            dl->PopClipRect();
        } else {
            dl->AddText(f, fSz, tp, greyTextCol, txt);
            if (fillW2 > 1.f) {
                draw_text_glow(fillW2);
                dl->PushClipRect(ImVec2(curX, curY), ImVec2(curX + fillW2, curY + logoH), true);
                dl->AddText(f, fSz, tp, IM_COL32(255,255,255, (int)(fAlpha*255)), txt);
                dl->PopClipRect();
            }
        }
    }
    if (!is_ready) {
        const char* st = phase_label ? phase_label : "";
        ImFont* f2 = ImGui::GetFont();
        float s2 = f2->CalcTextSizeA(12.f, FLT_MAX, 0.f, st).x;
        ImGui::GetWindowDrawList()->AddText(f2, 12.f, ImVec2(display.x*0.5f - s2*0.5f, curY + logoH + 12.f), IM_COL32(200,200,210, (int)(dimAlpha*200)), st);
        float barW = 320.f * UIScale, barH = 2.f;
        float bx = display.x*0.5f - barW*0.5f;
        float by = curY + logoH + 28.f;
        ImDrawList* dl2 = ImGui::GetWindowDrawList();
        dl2->AddRectFilled(ImVec2(bx,by), ImVec2(bx+barW, by+barH), IM_COL32(30,30,35, (int)(dimAlpha*180)), 1.f);
        float fw = barW * ImClamp(progress,0.f,1.f);
        if (fw > 1.f) dl2->AddRectFilled(ImVec2(bx,by), ImVec2(bx+fw, by+barH), IM_COL32(255,255,255, (int)(dimAlpha*220)), 1.f);
    }
    ImGui::End();
}
#endif

void Menu::DrawWatermark()
{
    if (!settings::menu::watermark)
        return;

    ID3D11ShaderResourceView* logo = ((ID3D11ShaderResourceView*)nullptr);
    const bool use_logo = settings::watermark::show_cheat_name && logo;

    static const char* separators[] = { " | ", " - ", " / ", " :: ", "  ", " \xC2\xB7 " };
    const char* sep = separators[std::clamp(settings::watermark::separator_type, 0, 5)];

    std::string name_line;
    if (settings::watermark::show_cheat_name && !use_logo)
        name_line = external_config::cheat_name;

    if (settings::watermark::show_display_name || settings::watermark::show_username)
    {
        std::string player_str;
        if (cache::cached_local_player.instance.address != 0)
        {
            if (settings::watermark::show_display_name && !cache::cached_local_player.display_name.empty())
                player_str = cache::cached_local_player.display_name;
            if (settings::watermark::show_username && !cache::cached_local_player.name.empty())
            {
                if (!player_str.empty())
                    player_str += " (@" + cache::cached_local_player.name + ")";
                else
                    player_str = "@" + cache::cached_local_player.name;
            }
        }
        if (!player_str.empty())
        {
            if (!name_line.empty()) name_line += sep;
            name_line += player_str;
        }
    }

    std::string stats_line;
    auto append_stat = [&](const std::string& s) {
        if (s.empty()) return;
        if (!stats_line.empty()) stats_line += sep;
        stats_line += s;
    };

    if (settings::watermark::show_fps)
    {
        float fps = -1.f;
        try
        {
            static uintptr_t rs_addr = 0;
            static uintptr_t rs_dm = 0;
            if (game::datamodel.address)
            {
                if (rs_dm != game::datamodel.address || !rs_addr)
                {
                    rbx::instance_t rs = game::datamodel.find_first_child_by_class("RunService");
                    rs_addr = rs.address;
                    rs_dm = game::datamodel.address;
                }
                if (rs_addr)
                {
                    const float hb = memory->read<float>(rs_addr + OFFSET(RunService, HeartbeatFPS));
                    if (hb > 5.f && hb < 1000.f)
                        fps = hb;
                    else if (hb > 0.0005f && hb < 1.f)
                        fps = 1.f / hb;
                }
            }
        }
        catch (...) {}

        if (fps < 0.f)
        {
            const float dt = ImGui::GetIO().DeltaTime;
            static float s_fps = 0.f;
            const float inst = dt > 1e-4f ? (1.f / dt) : 0.f;
            s_fps = (s_fps < 1.f) ? inst : (s_fps * 0.75f + inst * 0.25f);
            fps = s_fps;
        }

        char fpsbuf[24];
        snprintf(fpsbuf, sizeof(fpsbuf), "%d fps", (int)(fps + 0.5f));
        append_stat(fpsbuf);
    }

    if (settings::watermark::show_ping)
    {
        try
        {
            if (game::datamodel.address)
            {
                rbx::instance_t stats = game::datamodel.find_first_child_by_class("Stats");
                if (stats.address)
                {
                    rbx::instance_t network = stats.find_first_child("Network");
                    if (network.address)
                    {
                        rbx::instance_t ping_item = network.find_first_child("ServerStatsItem");
                        if (ping_item.address)
                        {
                            float ping_val = memory->read<float>(ping_item.address + Offsets::StatsItem::Value);
                            append_stat(std::to_string(static_cast<int>(ping_val * 1000.0f)) + "ms");
                        }
                    }
                }
            }
        }
        catch (...) {}
    }

    const bool want_stack = !name_line.empty() && !stats_line.empty()
        && (settings::watermark::show_display_name || settings::watermark::show_username)
        && settings::watermark::show_fps && settings::watermark::show_ping;

    static float stack_t = 0.f;
    {
        const float dt = ImClamp(ImGui::GetIO().DeltaTime, 0.f, 0.05f);
        const float spd = 7.5f;
        stack_t = ImClamp(stack_t + (want_stack ? dt : -dt) * spd, 0.f, 1.f);
    }
    const float ease = stack_t * stack_t * (3.f - 2.f * stack_t);

    ImU32 text_col;
    if (settings::watermark::rainbow)
    {
        float t = static_cast<float>(ImGui::GetTime()) * settings::watermark::rainbow_speed;
        float r = sinf(t) * 0.5f + 0.5f;
        float g = sinf(t + 2.094f) * 0.5f + 0.5f;
        float b = sinf(t + 4.189f) * 0.5f + 0.5f;
        text_col = IM_COL32(static_cast<int>(r * 255), static_cast<int>(g * 255), static_cast<int>(b * 255), 255);
    }
    else
    {
        text_col = ImGui::ColorConvertFloat4ToU32({
            settings::watermark::text_color[0],
            settings::watermark::text_color[1],
            settings::watermark::text_color[2],
            settings::watermark::text_color[3]
        });
    }

    ImFont* wm_font = MenuTheme::Fonts::SemiBold ? MenuTheme::Fonts::SemiBold : ImGui::GetFont();
    const float wm_font_sz = 16.f;
    const float pad_x = 16.f;
    const float pad_y = 12.f;
    const float logo_gap = 12.f;
    const float line_gap = 3.f;
    const float glow = 6.f;
    const ImVec2 name_sz = name_line.empty() ? ImVec2(0, 0)
        : wm_font->CalcTextSizeA(wm_font_sz, FLT_MAX, 0.f, name_line.c_str());
    const ImVec2 stats_sz = stats_line.empty() ? ImVec2(0, 0)
        : wm_font->CalcTextSizeA(wm_font_sz, FLT_MAX, 0.f, stats_line.c_str());
    const float sep_w = wm_font->CalcTextSizeA(wm_font_sz, FLT_MAX, 0.f, sep).x;
    const bool has_name = name_sz.x > 0.f;
    const bool has_stats = stats_sz.x > 0.f;

    const float logoH = ImLerp(32.f, 46.f, ease);
    const float logoW = use_logo ? logoH * (2172.f / 724.f) : 0.f;
    const float single_text_w = name_sz.x + (has_name && has_stats ? sep_w : 0.f) + stats_sz.x;
    const float stack_text_w = ImMax(name_sz.x, stats_sz.x);
    const float text_col_w = ImLerp(single_text_w, stack_text_w, ease);
    const float single_text_h = ImMax(name_sz.y, stats_sz.y);
    const float stack_text_h = name_sz.y + ((has_name && has_stats) ? line_gap : 0.f) + stats_sz.y;
    const float text_col_h = ImLerp(single_text_h, stack_text_h, ease);
    const float content_w = logoW + ((logoW > 0.f && text_col_w > 0.f) ? logo_gap : 0.f) + text_col_w;
    const float content_h = ImMax(logoW > 0.f ? logoH : 0.f, text_col_h);
    const float inner_w = content_w + pad_x * 2.f;
    const float inner_h = content_h + pad_y * 2.f;
    const float wm_width = inner_w + glow * 2.f;
    const float wm_height = inner_h + glow * 2.f;

    ImGuiWindowFlags wm_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    if (!m_bMenuVisible)
        wm_flags |= ImGuiWindowFlags_NoInputs;

    ImGui::SetNextWindowSize(ImVec2(wm_width, wm_height), ImGuiCond_Always);
    const ImVec2 wm_def(settings::watermark::pos_x, settings::watermark::pos_y);
    if (m_bMenuVisible)
        ImGui::SetNextWindowPos(MenuTheme::TickSmoothDrag("##QuannwareWatermark", ImVec2(wm_width, wm_height), wm_def, wm_height), ImGuiCond_Always);
    else if (!settings::watermark::pos_initialized)
        ImGui::SetNextWindowPos(wm_def, ImGuiCond_Always);
    settings::watermark::pos_initialized = true;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    bool wm_open = ImGui::Begin("##QuannwareWatermark", (bool*)0, wm_flags);
    ImGui::PopStyleVar(2);

    if (wm_open)
    {
        ImVec2 pos = ImGui::GetWindowPos();
        settings::watermark::pos_x = pos.x;
        settings::watermark::pos_y = pos.y;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImRect bar(ImVec2(pos.x + glow, pos.y + glow),
            ImVec2(pos.x + wm_width - glow, pos.y + wm_height - glow));
        dl->AddRectFilled(bar.Min, bar.Max, MenuTheme::ColSurfaceElevated(), 5.f);
        dl->AddRect(bar.Min, bar.Max, MenuTheme::ColBorder(), 5.f, 0, 1.f);

        const float cx = ImFloor(bar.Min.x + (bar.GetWidth() - content_w) * 0.5f);
        const float cy = ImFloor(bar.Min.y + (bar.GetHeight() - content_h) * 0.5f);
        float cursor_x = cx;

        if (use_logo)
        {
            const ImVec2 p0(cursor_x, ImFloor(cy + (content_h - logoH) * 0.5f));
            const ImVec2 p1(p0.x + logoW, p0.y + logoH);
            const float elapsed = (float)ImGui::GetTime();
            const float pulse = 0.82f + 0.12f * sinf(elapsed * 3.5f);
            const float baseSpread = 1.7f;
            for (int ring = 3; ring >= 1; --ring)
            {
                const float spread = baseSpread * ring * 0.5f;
                const int alpha = (int)(pulse * (16.f / ring));
                if (alpha <= 0) continue;
                const ImU32 glowCol = IM_COL32(255, 255, 255, alpha);
                const ImVec2 dirs[] = {
                    { spread, 0.f }, { -spread, 0.f }, { 0.f, spread }, { 0.f, -spread },
                    { spread, spread }, { -spread, spread }, { spread, -spread }, { -spread, -spread },
                };
                for (const ImVec2& d : dirs)
                    dl->AddImage((ImTextureID)logo,
                        ImVec2(p0.x + d.x, p0.y + d.y), ImVec2(p1.x + d.x, p1.y + d.y),
                        ImVec2(0, 0), ImVec2(1, 1), glowCol);
            }
            dl->AddImage((ImTextureID)logo, p0, p1, ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255));
            cursor_x = p1.x + logo_gap;
        }

        const float text_top = ImFloor(cy + (content_h - text_col_h) * 0.5f);
        const float name_x = ImFloor(ImLerp(cursor_x, cursor_x + (text_col_w - name_sz.x) * 0.5f, ease));
        const float name_y = text_top;
        const float stats_x = ImFloor(ImLerp(
            cursor_x + name_sz.x + (has_name && has_stats ? sep_w : 0.f),
            cursor_x + (text_col_w - stats_sz.x) * 0.5f,
            ease));
        const float stats_y = ImFloor(ImLerp(
            text_top + (text_col_h - stats_sz.y) * 0.5f,
            text_top + name_sz.y + (has_name ? line_gap : 0.f),
            ease));

        if (has_name)
            dl->AddText(wm_font, wm_font_sz, ImVec2(name_x, name_y), text_col, name_line.c_str());

        if (has_name && has_stats && ease < 0.98f)
        {
            const int sep_a = (int)((1.f - ease) * 255.f);
            ImU32 sep_col = (text_col & 0x00FFFFFF) | ((ImU32)sep_a << 24);
            dl->AddText(wm_font, wm_font_sz, ImVec2(ImFloor(cursor_x + name_sz.x), name_y), sep_col, sep);
        }

        if (has_stats)
            dl->AddText(wm_font, wm_font_sz, ImVec2(stats_x, stats_y), text_col, stats_line.c_str());
    }
    ImGui::End();
}

void Menu::DrawMenu()
{    static bool m_bMainWindowOpen = true;
    static bool m_bClientWindowOpen = true;
    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // 0=Combat  1=Visuals  2=Rage  3=Config  4=Settings
    static int nav_page = 0;

    // Sync dynamic accent color every frame
    MenuTheme::ApplyAccent();
    MenuTheme::ApplyLayoutScale();

    OpenCloseAnim::Tick();
    const float menu_ease = OpenCloseAnim::Ease();
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, menu_ease);

    // -- Background dim + falling rain (outside menu only) --------------------
    {
        const ImVec2 display = ImGui::GetIO().DisplaySize;

        // Track the actual menu position ? updated every frame after the window exists
        static ImVec2 s_mMin = ImFloor(display * 0.5f - ImVec2(MenuTheme::GetWindowW(), MenuTheme::GetWindowH()) * 0.5f);
        ImGuiWindow* mw = ImGui::FindWindowByName("##MatchaMain");
        if (mw) s_mMin = ImFloor(mw->Pos);
        const ImVec2 mMin = s_mMin;
        const ImVec2 mMax = mMin + (mw ? mw->Size : ImVec2(MenuTheme::GetWindowW(), MenuTheme::GetWindowH()));

        ImDrawList* bg = ImGui::GetBackgroundDrawList();

        // Full-screen dim — driven by bg_opacity setting, faded with the menu
        bg->AddRectFilled(ImVec2(0.f, 0.f), display,
            IM_COL32(0, 0, 0, static_cast<int>(settings::menu::bg_opacity * 255.f * menu_ease)));

        // Falling snow — depth-sorted flakes: small/dim/slow = far, large/bright/fast = near
        if (settings::menu::rain_effect && settings::menu::rain_count > 0)
        {
            struct Flake { float x_frac, y, depth, phase; };
            static Flake s_flakes[160];
            static int   s_n = -1;
            const int n = ImClamp(settings::menu::rain_count, 0, 160);
            if (s_n != n)
            {
                s_n = n;
                for (int i = 0; i < n; ++i)
                {
                    const float u = (float)((i * 47 + 13) % 1000) * 0.001f;
                    s_flakes[i].depth  = u * u; // more far flakes than near
                    s_flakes[i].x_frac = (float)(i * 179 % 1000) * 0.001f;
                    s_flakes[i].y      = (float)(i * 137 % 1000) * 0.001f * display.y;
                    s_flakes[i].phase  = (float)((i * 29) % 628) * 0.01f;
                }
            }

            const float dt = ImGui::GetIO().DeltaTime;
            const float t  = (float)ImGui::GetTime();
            const float op = settings::menu::rain_opacity;
            const float wrap = display.y + 16.f;

            for (int i = 0; i < n; ++i)
            {
                Flake& f = s_flakes[i];
                const float d = f.depth;
                const float speed = 22.f + d * 130.f;
                f.y += speed * dt;
                if (f.y > wrap)
                {
                    f.y -= wrap + 16.f;
                    f.x_frac = fmodf(f.x_frac + 0.083f + d * 0.11f, 1.f);
                }

                const float sway = sinf(t * (0.35f + d * 1.4f) + f.phase) * (3.f + d * 16.f);
                const float x = f.x_frac * display.x + sway;
                const float y = f.y;
                const float pad = 6.f + d * 14.f;
                if (y < -pad || y > display.y + pad) continue;
                if (x >= mMin.x && x <= mMax.x && y >= mMin.y && y <= mMax.y) continue;

                float fade = 1.f;
                if (y < 16.f) fade = y * (1.f / 16.f);
                else if (y > display.y - 16.f) fade = (display.y - y) * (1.f / 16.f);
                if (fade < 0.02f) continue;

                const float size = 0.8f + d * 4.6f;
                const float a = fade * op * menu_ease * (0.22f + d * 0.78f);
                const ImVec2 c(x, y);
                const int segs = (d > 0.55f) ? 16 : 12;
                for (int L = 5; L >= 1; --L)
                {
                    const float u = (float)L / 5.f;
                    const float r = size * (0.28f + u * 2.35f);
                    const int al = (int)((12.f + (1.f - u) * (1.f - u) * 190.f) * a);
                    if (al < 2) continue;
                    bg->AddCircleFilled(c, r, IM_COL32(236, 240, 255, ImClamp(al, 0, 220)), segs);
                }
            }
        }
    }
    // -------------------------------------------------------------------------

    // -----------------------------------------------------------------------
    // Explorer satellite panel -- rendered BEFORE the main window so it
    // sits on top in ImGui's window stack and receives mouse input first.
    // -----------------------------------------------------------------------
    if (MenuTheme::show_explorer)
        explorer::explorer->render_window(&MenuTheme::show_explorer);
    if (MenuTheme::show_players)
        players::render_window(&MenuTheme::show_players);

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    MenuTheme::ShellRects shell{};
    const bool main_begun = MenuTheme::BeginMainWindow("##MatchaMain", &m_bMainWindowOpen, &shell, &nav_page);
    if (main_begun)
    {
        m_bMainWindowOpen = true;
        m_bMenuVisible = true;
        MenuTheme::DrawMainChrome(shell, &nav_page);

        // Top-select row (Favorites/Standard + hue slider) is drawn inside DrawMainChrome

        // -- Content Area ------------------------------------------------------
        // 12px outer padding + 12px gap between panels (ItemSpacing)
        // The outer wrapper is fully transparent ? individual panels float as
        // separate groups against the dark window background (screenshot 2 look).
        constexpr float kPadBase = 10.f;
        const float kPad = MenuTheme::Sz(kPadBase);
        const float kPadTop = MenuTheme::Sz(6.f);
        ImGui::SetCursorScreenPos(shell.content.Min + ImVec2(kPad, kPadTop));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(kPad, kPadTop));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,     ImVec2(kPad, kPad));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildPadding,    ImVec2(MenuTheme::Sz(14.f), MenuTheme::Sz(8.f)));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.f);
        const ImVec2 content_size = shell.content.GetSize() - ImVec2(kPad * 2.f, kPadTop + kPad);
        // Use plain BeginChild with NoBackground so no solid rectangle wraps all panels.
        MenuTheme::BeginContentFit();
        bool main_content_child = ImGui::BeginChild("##body", content_size,
            ImGuiChildFlags_AlwaysUseWindowPadding,
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar(4);

        if (main_content_child)
        {
            // Main tabs — quick fade out then new page slides down per-feature
            NavAnim::EnsureInit(nav_page);
            if (nav_page != NavAnim::Target())
                NavAnim::Request(nav_page);
            NavAnim::Update(ImGui::GetIO().DeltaTime);
            static int s_prev_display = NavAnim::Display();
            const int nav_display = NavAnim::Display();
            if (s_prev_display != nav_display && nav_display == 0)
                AimTabAnim::TriggerEntrance();
            s_prev_display = nav_display;

            // Gap between sibling section boxes (Aimbot/Colorbot/etc.) — matches
            // MenuTheme::Typography::kSectionSpacing (8px). Rows *within* a
            // section use a tighter 4px rhythm pushed by ImAdd::BeginChild.
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(MenuTheme::Sz(MenuTheme::Typography::kSectionSpacing), MenuTheme::Sz(MenuTheme::Typography::kSectionSpacing)));
            auto matcha_card = [](const char* id, float width, float height = 0.f) -> bool
            {
                const ImVec2 cp = ImGui::GetCursorScreenPos();
                ImGui::SetCursorScreenPos(ImVec2(IM_ROUND(cp.x), IM_ROUND(cp.y)));
                const ImVec4 slot(41/255.f, 46/255.f, 66/255.f, 1.f);   // #292e42
                const ImVec4 slot_h(50/255.f, 56/255.f, 66/255.f, 1.f); // #323842
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(32/255.f, 34/255.f, 46/255.f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_FrameBg, slot);
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, slot_h);
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, slot_h);
                ImGui::PushStyleColor(ImGuiCol_Button, slot);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, slot_h);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, slot_h);
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f);
                ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.f, 12.f));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.f, 8.f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f);
                ImGuiChildFlags flags = ImGuiChildFlags_AlwaysUseWindowPadding;
                ImVec2 sz(width, height);
                if (height <= 0.f)
                {
                    sz.y = 0.f;
                    flags |= ImGuiChildFlags_AutoResizeY;
                }
                const bool open = ImGui::BeginChild(id, sz, flags,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                ImGui::PushItemWidth(-1.f);
                return open;
            };
            auto matcha_card_end = []()
            {
                ImGui::PopItemWidth();
                ImGui::EndChild();
                ImGui::PopStyleVar(5);
                ImGui::PopStyleColor(8);
            };
            auto card_rule = []()
            {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                const ImVec2 wp = ImGui::GetWindowPos();
                const float ly = IM_ROUND(ImGui::GetCursorScreenPos().y);
                const float inset = ImGui::GetStyle().WindowPadding.x;
                dl->AddLine(ImVec2(wp.x + inset, ly), ImVec2(wp.x + ImGui::GetWindowWidth() - inset, ly),
                    IM_COL32(41, 46, 66, 255), 1.f);
                ImGui::Dummy(ImVec2(1.f, 10.f));
            };
            auto inner_tabs = [&](const char* const* labels, int count, int* cur)
            {
                ImFont* f = MenuTheme::Fonts::SemiBold ? MenuTheme::Fonts::SemiBold : ImGui::GetFont();
                const float fsz = MenuTheme::FontPx(13.f);
                ImDrawList* dl = ImGui::GetWindowDrawList();
                float x = ImGui::GetCursorScreenPos().x;
                const float y = ImGui::GetCursorScreenPos().y;
                for (int i = 0; i < count; ++i)
                {
                    const ImVec2 tsz = f->CalcTextSizeA(fsz, FLT_MAX, 0.f, labels[i]);
                    const ImVec2 tmin(x, y);
                    const ImVec2 tmax(x + tsz.x + 12.f, y + 20.f);
                    ImGui::SetCursorScreenPos(tmin);
                    if (ImGui::InvisibleButton(labels[i], tmax - tmin))
                        *cur = i;
                    if (*cur == i)
                        MenuTheme::AddRectFilledCrisp(dl, ImVec2(x - 6.f, y - 1.f), ImVec2(x + tsz.x + 6.f, y + 18.f),
                            IM_COL32(26, 27, 37, 255), 9.f);
                    dl->AddText(f, fsz, ImVec2(x, y + 2.f),
                        (*cur == i) ? IM_COL32(0, 0, 0, 255) : IM_COL32(64, 70, 102, 255), labels[i]);
                    x += tsz.x + 16.f;
                }
                ImGui::SetCursorScreenPos(ImVec2(ImGui::GetWindowPos().x + ImGui::GetStyle().WindowPadding.x, y + 24.f));
                card_rule();
            };
            auto card_title = [&](const char* title)
            {
                ImFont* f = MenuTheme::Fonts::SemiBold ? MenuTheme::Fonts::SemiBold : ImGui::GetFont();
                ImGui::GetWindowDrawList()->AddText(f, MenuTheme::FontPx(13.f), ImGui::GetCursorScreenPos(),
                    IM_COL32(122, 131, 165, 255), title);
                ImGui::Dummy(ImVec2(1.f, 22.f));
                card_rule();
            };
            auto checkbox_colors2 = [](const char* label, bool* v, float* c0, float* c1) -> bool
            {
                const float avail = ImGui::GetContentRegionAvail().x;
                const ImVec2 row = ImGui::GetCursorScreenPos();
                const float sw = ImAdd::kColorSwatchWidth;
                const float gap = ImAdd::kColorSwatchGap;
                const bool changed = ImAdd::CheckBox(label, v, sw * 2.f + gap * 2.f);
                char id0[80], id1[80];
                ImFormatString(id0, IM_ARRAYSIZE(id0), "##c0_%s", label);
                ImFormatString(id1, IM_ARRAYSIZE(id1), "##c1_%s", label);
                const float row_h = ImGui::GetItemRectSize().y;
                ImGui::SetCursorScreenPos(ImVec2(row.x + avail - sw * 2.f - gap, row.y + (row_h - ImAdd::kColorSwatchSize) * 0.5f));
                ImAdd::ColorEdit4(id0, c0);
                ImGui::SameLine(0, gap);
                ImAdd::ColorEdit4(id1, c1);
                ImGui::SetCursorScreenPos(ImVec2(row.x, ImGui::GetItemRectMax().y + ImGui::GetStyle().ItemSpacing.y));
                return changed;
            };

            if (nav_display == 0)  // --- COMBAT ---
            {
                NavAnim::BeginPanel();
                const float col_gap = 18.f;
                const float row_gap = 4.f;
                const float avail_w = ImGui::GetContentRegionAvail().x;
                const float col_w = ImTrunc((avail_w - col_gap) * 0.5f);

                ImGui::BeginGroup();
                if (matcha_card("##card_aimbot", col_w))
                {
                    static int aim_sub = 0;
                    const char* atabs[] = { "Aimbot", "Prediction", "Smoothness", "FOV" };
                    inner_tabs(atabs, 4, &aim_sub);
                    static ImGuiKey aimbot_key = ImGuiKey_None;
                    aimbot_key = keybind::vk_to_imgui_key(settings::aimbot::keybind);
                    if (aim_sub == 0)
                    {
                        if (ImAdd::CheckBoxKeyBind("Enabled", &settings::aimbot::enabled, &aimbot_key, &settings::aimbot::activation_mode))
                            settings::aimbot::keybind = keybind::imgui_key_to_vk(aimbot_key);
                        ImAdd::CheckBox("Team Check", &settings::aimbot::teamcheck);
                        ImAdd::CheckBox("Visible Check", &settings::aimbot::visibility_check);
                        ImAdd::CheckBox("Health Check", &settings::aimbot::health_check_enabled);
                        ImAdd::CheckBox("Sticky Aim", &settings::aimbot::sticky_aim);
                        settings::aimbot::max_range_enabled = true;
                        ImAdd::SliderFloat("Distance", &settings::aimbot::max_range, 10.f, 2000.f, "%.0f");
                        ImAdd::SliderFloat("Sensitivity", &settings::aimbot::roblox_sensitivity, 0.1f, 1.f, "%.2f");
                        ImAdd::Combo("Hit Part", &settings::aimbot::target_part, { "Closest", "Head", "HumanoidRootPart", "LeftArm", "RightArm", "LeftLeg", "RightLeg" });
                        ImAdd::Combo("Aim Type", &settings::aimbot::mode, { "Mouse", "Camera" });
                        ImAdd::CheckBox("Rage Method", &settings::aimbot::rage_method);
                        ImAdd::Combo("Type", &settings::aimbot::type, { "Mouse", "Camera Teleport" });
                    }
                    else if (aim_sub == 1)
                    {
                        ImAdd::CheckBox("Enabled##ab_pred", &settings::aimbot::enable_prediction);
                        ImAdd::SliderFloat("X (Division)##ab_pred", &settings::aimbot::prediction_x, 0.f, 20.f, "%.2f");
                        ImAdd::SliderFloat("Y (Division)##ab_pred", &settings::aimbot::prediction_y, 0.f, 20.f, "%.2f");
                    }
                    else if (aim_sub == 2)
                    {
                        ImAdd::CheckBox("Enabled##ab_smooth", &settings::aimbot::smoothing);
                        ImAdd::SliderFloat("Smoothness X##ab", &settings::aimbot::smoothingx, 1.f, 100.f, "%.1f");
                        ImAdd::SliderFloat("Smoothness Y##ab", &settings::aimbot::smoothingy, 1.f, 100.f, "%.1f");
                    }
                    else
                    {
                        if (ImAdd::CheckBoxColor("Enabled##ab_fov", &settings::aimbot::draw_fov, settings::aimbot::fov_circle_colour))
                            settings::aimbot::use_fov = settings::aimbot::draw_fov;
                        ImAdd::CheckBox("Glow##ab_fov", &settings::aimbot::fov_glow);
                        ImAdd::CheckBox("Filled##ab_fov", &settings::aimbot::fov_filled);
                        ImAdd::SliderFloat("Size##ab_fov", &settings::aimbot::fov, 0.f, 1000.f, "%.0f");
                        ImAdd::Combo("Style##ab_fov", &settings::aimbot::fov_style, { "Smooth", "Hexagon", "Square" });
                    }
                    matcha_card_end();
                }
                ImGui::Dummy(ImVec2(0, row_gap));
                if (matcha_card("##card_misc", col_w))
                {
                    ImFont* f = MenuTheme::Fonts::SemiBold ? MenuTheme::Fonts::SemiBold : ImGui::GetFont();
                    ImGui::GetWindowDrawList()->AddText(f, MenuTheme::FontPx(13.f), ImGui::GetCursorScreenPos(), IM_COL32(122, 131, 165, 255), "Misc");
                    ImGui::Dummy(ImVec2(1.f, 22.f));
                    card_rule();
                    ImAdd::CheckBox("Resolver", &settings::aimbot::resolver);
                    matcha_card_end();
                }
                ImGui::EndGroup();
                ImGui::SameLine(0, col_gap);
                ImGui::BeginGroup();
                if (matcha_card("##card_sa", col_w))
                {
                    static int sa_sub = 0;
                    const char* stabs[] = { "Silent Aim", "Prediction", "FOV" };
                    inner_tabs(stabs, 3, &sa_sub);
                    static ImGuiKey silentaim_key = ImGuiKey_None;
                    silentaim_key = keybind::vk_to_imgui_key(settings::silentaim::keybind);
                    if (sa_sub == 0)
                    {
                        if (ImAdd::CheckBoxKeyBind("Enabled", &settings::silentaim::enabled, &silentaim_key, &settings::silentaim::activation_mode))
                            settings::silentaim::keybind = keybind::imgui_key_to_vk(silentaim_key);
                        ImAdd::CheckBox("Team Check", &settings::silentaim::teamcheck);
                        ImAdd::CheckBox("Visible Check", &settings::silentaim::visibility_check);
                        ImAdd::CheckBox("Health Check", &settings::silentaim::health_check_enabled);
                        ImAdd::CheckBox("Sticky Aim", &settings::silentaim::sticky_aim);
                        ImAdd::SliderFloat("Distance", &settings::silentaim::max_range, 10.f, 2000.f, "%.0f");
                        ImAdd::Combo("Hit Part", &settings::silentaim::target_part, { "Nearest Point", "Closest", "Head", "HumanoidRootPart", "LeftArm", "RightArm", "LeftLeg", "RightLeg" });
                        ImAdd::Combo("Method", &settings::silentaim::method, { "Experimental" });
                    }
                    else if (sa_sub == 1)
                    {
                        ImAdd::CheckBox("Enabled##sa_pred", &settings::silentaim::enable_prediction);
                        ImAdd::SliderFloat("X (Division)##sa_pred", &settings::silentaim::prediction_x, 0.f, 20.f, "%.2f");
                        ImAdd::SliderFloat("Y (Division)##sa_pred", &settings::silentaim::prediction_y, 0.f, 20.f, "%.2f");
                    }
                    else
                    {
                        if (ImAdd::CheckBoxColor("Enabled##sa_fov", &settings::silentaim::draw_fov, settings::silentaim::fov_circle_colour))
                            settings::silentaim::use_fov = settings::silentaim::draw_fov;
                        ImAdd::CheckBox("Glow##sa_fov", &settings::silentaim::fov_glow);
                        ImAdd::CheckBox("Filled##sa_fov", &settings::silentaim::fov_filled);
                        ImAdd::SliderFloat("Size##sa_fov", &settings::silentaim::fov, 0.f, 1000.f, "%.0f");
                        ImAdd::Combo("Style##sa_fov", &settings::silentaim::fov_style, { "Smooth", "Hexagon", "Square" });
                    }
                    matcha_card_end();
                }
                ImGui::Dummy(ImVec2(0, row_gap));
                if (matcha_card("##card_tb", col_w))
                {
                    ImFont* f = MenuTheme::Fonts::SemiBold ? MenuTheme::Fonts::SemiBold : ImGui::GetFont();
                    ImGui::GetWindowDrawList()->AddText(f, MenuTheme::FontPx(13.f), ImGui::GetCursorScreenPos(), IM_COL32(122, 131, 165, 255), "Trigger Bot");
                    ImGui::Dummy(ImVec2(1.f, 22.f));
                    card_rule();
                    static ImGuiKey tb_key = ImGuiKey_None;
                    tb_key = keybind::vk_to_imgui_key(settings::triggerbot::keybind);
                    if (ImAdd::CheckBoxKeyBind("Enabled", &settings::triggerbot::enabled, &tb_key, &settings::triggerbot::activation_mode))
                        settings::triggerbot::keybind = keybind::imgui_key_to_vk(tb_key);
                    ImAdd::CheckBox("Visible Check", &settings::triggerbot::require_los);
                    ImAdd::CheckBox("Team Check", &settings::triggerbot::teamcheck);
                    ImAdd::SliderFloat("Hitbox Mul", &settings::rage::hitbox_expander::size_x, 0.5f, 8.f, "%.2f");
                    ImAdd::SliderFloat("Delay (ms)", &settings::triggerbot::delay_ms, 0.f, 500.f, "%.0f");
                    ImAdd::SliderFloat("Release (ms)", &settings::triggerbot::hold_ms, 1.f, 500.f, "%.0f");
                    matcha_card_end();
                }
                ImGui::EndGroup();
            }
            else if (nav_display == 1)  // --- VISUALS ---
            {
                NavAnim::BeginPanel();
                const float col_gap = 18.f;
                const float avail_w = ImGui::GetContentRegionAvail().x;
                const float col_w = ImTrunc((avail_w - col_gap) * 0.5f);
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 10.f));

                ImGui::BeginGroup();
                if (matcha_card("##card_esp", col_w))
                {
                    static int esp_sub = 0;
                    const char* etabs[] = { "ESP", "Crosshair", "Misc", "Flaggs" };
                    inner_tabs(etabs, 4, &esp_sub);
                    if (esp_sub == 0)
                    {
                        ImAdd::CheckBox("Enabled##esp_en", &settings::visuals::enable_enemies);
                        ImAdd::CheckBox("Team Check##esp_team", &settings::visuals::teamcheck);
                        checkbox_colors2("Visible Check##esp_vis", &settings::visuals::visibility_check,
                            settings::visuals::visible_color, settings::visuals::occluded_color);
                        ImAdd::CheckBox("Team Based Color##esp_tbc", &settings::visuals::team_based_color);
                        ImAdd::CheckBoxColor("Text Gradient##esp_tg", &settings::visuals::blend, settings::visuals::name_color_blend_start);
                        ImAdd::CheckBoxColor("Text Background##esp_tb", &settings::visuals::text_background, settings::visuals::text_background_color);
                        ImAdd::CheckBox("Outline##esp_out", &settings::visuals::text_shadow);
                        ImAdd::CheckBox("Glow##esp_glow", &settings::visuals::esp_glow);
                        ImAdd::CheckBox("Self ESP##esp_self", &settings::visuals::enable_client);
                        ImAdd::Combo("Sizing Type##esp_size", &settings::visuals::box_type, { "Bounding", "Cornered" });
                        settings::visuals::max_distance_enabled = true;
                        ImAdd::SliderFloat("Render Distance##esp_rd", &settings::visuals::max_distance, 1.f, 3000.f, "%.0f");
                    }
                    else if (esp_sub == 1)
                    {
                        ImAdd::CheckBoxColor("Enabled##xh", &settings::crosshair::enabled, settings::crosshair::color);
                        ImAdd::CheckBox("Rainbow##xh", &settings::crosshair::rainbow);
                        ImAdd::Combo("Style##xh", &settings::crosshair::style, { "Dots", "Lines", "Static" });
                        ImAdd::SliderFloat("Radius##xh", &settings::crosshair::radius, 2.f, 40.f, "%.1f");
                        ImAdd::SliderFloat("Thickness##xh", &settings::crosshair::thickness, 0.5f, 6.f, "%.1f");
                    }
                    else if (esp_sub == 2)
                    {
                        ImAdd::CheckBox("Filter Dead##esp_fd", &settings::visuals::filter_dead);
                        ImAdd::CheckBox("Filter Invisible##esp_fi", &settings::visuals::filter_invisible);
                        ImAdd::CheckBox("Ignore Whitelisted##esp_iw", &settings::visuals::ignore_whitelisted);
                        ImAdd::CheckBox("Knock Check##esp_kc", &settings::visuals::knock_check);
                        ImAdd::Combo("Font##esp_font", &settings::visuals::esp_font, { "Tahoma", "Smallest Pixel", "Arial" });
                        ImAdd::SliderFloat("Fade In##esp_fi_spd", &settings::visuals::fade_in_speed, 0.1f, 50.f, "%.1f");
                        ImAdd::SliderFloat("Fade Out##esp_fo_spd", &settings::visuals::fade_out_speed, 0.1f, 50.f, "%.1f");
                    }
                    else
                    {
                        ImAdd::CheckBoxColor("Enabled##flags", &settings::visuals::flags, settings::visuals::flags_state_colour);
                    }
                    matcha_card_end();
                }
                if (matcha_card("##card_box", col_w))
                {
                    card_title("Box");
                    ImAdd::CheckBoxColor("Enabled##box", &settings::visuals::box, settings::visuals::box_color);
                    ImAdd::CheckBoxColor("Fill Box##boxf", &settings::visuals::box_fill, settings::visuals::box_fill_color);
                    ImAdd::Combo("Box Type##box", &settings::visuals::box_type, { "2D", "Cornered" });
                    matcha_card_end();
                }
                if (matcha_card("##card_name", col_w))
                {
                    card_title("Name");
                    ImAdd::CheckBoxColor("Enabled##name", &settings::visuals::name, settings::visuals::name_color);
                    ImAdd::Combo("Type##name", &settings::visuals::name_display_type, { "Name", "Username" });
                    matcha_card_end();
                }
                ImGui::EndGroup();
                ImGui::SameLine(0, col_gap);
                ImGui::BeginGroup();
                if (matcha_card("##card_ind", col_w))
                {
                    static int ind_sub = 0;
                    const char* itabs[] = { "Indicators", "OOF Arrow", "Radar" };
                    inner_tabs(itabs, 3, &ind_sub);
                    if (ind_sub == 0)
                    {
                        ImAdd::CheckBoxColor("Distance##ind", &settings::visuals::distance, settings::visuals::distance_color);
                        ImAdd::CheckBoxColor("Equipped Item##ind", &settings::visuals::tool, settings::visuals::tool_color);
                        ImAdd::CheckBoxColor("Skeleton##ind", &settings::visuals::skeleton, settings::visuals::skeleton_color);
                        ImAdd::CheckBoxColor("Head Dot##ind", &settings::visuals::head_dot, settings::visuals::head_dot_color);
                        ImAdd::CheckBox("Head Dot Glow##ind", &settings::visuals::head_dot_glow);
                        ImAdd::CheckBox("Profile Picture##ind", &settings::visuals::avatar);
                    }
                    else if (ind_sub == 1)
                    {
                        ImAdd::CheckBoxColor("Enabled##oof", &settings::visuals::hit_tracers_enabled, settings::visuals::hit_tracers_color);
                    }
                    else
                    {
                        ImAdd::CheckBox("Enabled##radar", &settings::visuals::radar_enabled);
                        ImAdd::SliderFloat("Size##radar", &settings::visuals::radar_size, 50.f, 300.f, "%.0f");
                    }
                    matcha_card_end();
                }
                if (matcha_card("##card_hp", col_w))
                {
                    card_title("Health");
                    ImAdd::CheckBoxColor("Health Bar##hp", &settings::visuals::healthbar, settings::visuals::healthbar_color);
                    ImAdd::CheckBox("Health Based##hp", &settings::visuals::health_based_healthbar);
                    ImAdd::CheckBox("Health Text##hp", &settings::visuals::health_percent);
                    ImAdd::Combo("Text Pos##hp", &settings::visuals::health_text_pos, { "Above Name", "Below Name", "Right" });
                    matcha_card_end();
                }
                if (matcha_card("##card_chams", col_w))
                {
                    card_title("Chams");
                    static int chams_kind = 0;
                    ImAdd::Combo("Mode##chams", &chams_kind, { "Default", "Shader", "Engine", "Mesh", "Native" });
                    bool* chams_on =
                        (chams_kind == 1) ? &settings::visuals::shader_chams_enabled :
                        (chams_kind == 3) ? &settings::visuals::mesh_chams_enabled :
                        (chams_kind == 4) ? &settings::visuals::native_chams_enabled :
                                            &settings::visuals::engine_chams_enabled;
                    if (checkbox_colors2("Enabled##chams", chams_on, settings::visuals::mesh_chams_fill_color, settings::visuals::mesh_chams_outline_color))
                    {
                        const bool on = *chams_on;
                        settings::visuals::shader_chams_enabled = on && (chams_kind == 1);
                        settings::visuals::engine_chams_enabled = on && (chams_kind == 0 || chams_kind == 2);
                        settings::visuals::mesh_chams_enabled = on && (chams_kind == 3);
                        settings::visuals::native_chams_enabled = on && (chams_kind == 4);
                        settings::visuals::world_render_enabled = false;
                    }
                    ImAdd::CheckBoxColor("Filled##chams", &settings::visuals::mesh_chams_outline_enabled, settings::visuals::mesh_chams_fill_color);
                    int render_type = settings::visuals::native_chams_animated ? 1 : 0;
                    if (ImAdd::Combo("Rendering Type##chams", &render_type, { "Static", "Animated" }))
                        settings::visuals::native_chams_animated = (render_type == 1);
                    matcha_card_end();
                }
                if (matcha_card("##card_tracer", col_w))
                {
                    card_title("Tracer");
                    ImAdd::CheckBoxColor("Enabled##tr", &settings::visuals::snap_line, settings::visuals::snap_line_color);
                    ImAdd::Combo("Origin##tr", &settings::visuals::snap_line_origin, { "Bottom", "Top", "Cursor" });
                    matcha_card_end();
                }
                ImGui::EndGroup();
                ImGui::PopStyleVar();
            }
            else if (nav_display == 2)  // --- WORLD ---
            {
                NavAnim::BeginPanel();
                const float col_gap = 18.f;
                const float avail_w = ImGui::GetContentRegionAvail().x;
                const float col_w = ImTrunc((avail_w - col_gap) * 0.5f);
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 10.f));

                static bool freecam_on = false, freecam_tp = false, freecam_lock = false;
                static ImGuiKey freecam_key = ImGuiKey_I, freecam_tp_key = ImGuiKey_N;
                static int freecam_mode = 1, freecam_tp_mode = 1;
                static float freecam_speed = 1.50f;
                static char wp_name[64] = "";
                static bool wp_tween = false, wp_vis = false;
                static float wp_tween_spd = 200.f;
                static float glow_col[4] = { 1.f, 1.f, 1.f, 1.f };

                ImGui::BeginGroup();
                if (matcha_card("##card_cam", col_w))
                {
                    card_title("Camera Modification");
                    ImAdd::CheckBox("Camera Field Of View##cam", &settings::movement::thirdperson::enabled);
                    ImAdd::SliderFloat("Amount##cam", &settings::movement::thirdperson::fov, 1.f, 120.f, "%.0f");
                    matcha_card_end();
                }
                if (matcha_card("##card_freecam", col_w))
                {
                    card_title("Freecam");
                    ImAdd::CheckBoxKeyBind("Enabled##fc", &freecam_on, &freecam_key, &freecam_mode);
                    ImAdd::CheckBoxKeyBind("Teleport on click##fc", &freecam_tp, &freecam_tp_key, &freecam_tp_mode);
                    ImAdd::CheckBox("Lock Character Position##fc", &freecam_lock);
                    ImAdd::SliderFloat("Speed##fc", &freecam_speed, 0.1f, 10.f, "%.2f");
                    matcha_card_end();
                }
                if (matcha_card("##card_wp", col_w))
                {
                    card_title("Waypoint");
                    ImGui::Text("Name");
                    ImGui::InputText("##wp_name", wp_name, IM_ARRAYSIZE(wp_name));
                    ImAdd::Button("Create##wp", ImVec2(-1.f, 0.f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.28f, 0.28f, 1.f));
                    ImGui::TextUnformatted("No saved waypoints");
                    ImGui::PopStyleColor();
                    ImAdd::Button("Remove waypoint##wp", ImVec2(-1.f, 0.f));
                    ImAdd::Button("Goto##wp", ImVec2(-1.f, 0.f));
                    ImAdd::CheckBox("Tween goto##wp", &wp_tween);
                    ImAdd::SliderFloat("Tween speed##wp", &wp_tween_spd, 1.f, 500.f, "%.2f");
                    ImAdd::CheckBox("Visualize waypoint##wp", &wp_vis);
                    matcha_card_end();
                }
                ImGui::EndGroup();
                ImGui::SameLine(0, col_gap);
                ImGui::BeginGroup();
                if (matcha_card("##card_light", col_w))
                {
                    card_title("World Lighting");
                    ImAdd::CheckBoxColor("Ambience##wl", &settings::lighting::ambient::enabled, settings::lighting::ambient::ambient_color);
                    float fog_col[4] = {
                        settings::lighting::fog::fog_r, settings::lighting::fog::fog_g,
                        settings::lighting::fog::fog_b, 1.f };
                    ImAdd::CheckBoxColor("Custom Fog##wl", &settings::lighting::fog::enabled, fog_col);
                    settings::lighting::fog::fog_r = fog_col[0];
                    settings::lighting::fog::fog_g = fog_col[1];
                    settings::lighting::fog::fog_b = fog_col[2];
                    ImAdd::CheckBoxColor("Glow##wl", &settings::lighting::bloom::enabled, glow_col);
                    ImAdd::SliderFloat("Distance##wl", &settings::lighting::fog::fog_end, 0.f, 2000.f, "%.0f");
                    ImAdd::CheckBox("Custom Exposure##wl", &settings::lighting::exposure::enabled);
                    ImAdd::SliderFloat("Exposure Compensation##wl", &settings::lighting::exposure::exposure, -10.f, 10.f, "%.2f");
                    ImAdd::CheckBoxColor("Custom Brightness##wl", &settings::lighting::color_correction::enabled, settings::lighting::color_correction::tint);
                    ImAdd::SliderFloat("Brightness##wl", &settings::lighting::color_correction::brightness, -2.f, 2.f, "%.2f");
                    ImAdd::CheckBox("Custom Time##wl", &settings::lighting::clocktime::enabled);
                    ImAdd::SliderFloat("Clock Time##wl", &settings::lighting::clocktime::clock_time, 0.f, 24.f, "%.2f");
                    matcha_card_end();
                }
                ImGui::EndGroup();
                ImGui::PopStyleVar();
            }
            else if (nav_display == 3)  // --- CHARACTER ---
            {
                NavAnim::BeginPanel();
                const float col_gap = 18.f;
                const float avail_w = ImGui::GetContentRegionAvail().x;
                const float col_w = ImTrunc((avail_w - col_gap) * 0.5f);
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 10.f));

                static ImGuiKey desync_key = ImGuiKey_None;
                static ImGuiKey speed_key = ImGuiKey_None;
                static ImGuiKey fly_key = ImGuiKey_None;
                static ImGuiKey jump_key = ImGuiKey_None;
                static ImGuiKey ghost_key = ImGuiKey_None;
                static ImGuiKey hbe_key = ImGuiKey_J;
                static ImGuiKey orbit_key = ImGuiKey_X;
                static ImGuiKey noclip_key = ImGuiKey_Z;
                static ImGuiKey clicktp_key = ImGuiKey_LeftCtrl;
                static ImGuiKey bhop_key = ImGuiKey_None;
                static int hbe_mode = 1, orbit_mode = 1, noclip_mode = 1, clicktp_mode = 1;
                static bool hbe_team = false, hbe_hp = false, click_tp = false;
                static bool orbit_circle = false;
                static float orbit_circle_col[4] = { 1.f, 1.f, 1.f, 1.f };
                static bool ds_walk = false, ds_pos = false, ds_reset = false, ds_tick = false;
                static int ds_interval = 10;
                desync_key = keybind::vk_to_imgui_key(settings::movement::desync::keybind);
                speed_key  = keybind::vk_to_imgui_key(settings::movement::speedhack::keybind);
                fly_key    = keybind::vk_to_imgui_key(settings::movement::flyhack::keybind);
                jump_key   = keybind::vk_to_imgui_key(settings::movement::jumphack::keybind);
                ghost_key  = keybind::vk_to_imgui_key(settings::rage::ghost_mode::keybind);
                bhop_key   = keybind::vk_to_imgui_key(settings::movement::bhop::keybind);

                ImGui::BeginGroup();
                if (matcha_card("##card_hbe", col_w))
                {
                    card_title("Hitbox Extender");
                    ImAdd::CheckBoxKeyBind("Enabled##hbe", &settings::rage::hitbox_expander::enabled, &hbe_key, &hbe_mode);
                    ImAdd::CheckBox("Visualize Hitbox##hbe", &settings::visuals::view_hitbox);
                    ImAdd::CheckBox("Team Check##hbe", &hbe_team);
                    ImAdd::CheckBox("Health Check##hbe", &hbe_hp);
                    ImAdd::SliderFloat("Hitbox Size##hbe", &settings::rage::hitbox_expander::size_x, 1.f, 50.f, "%.0f");
                    settings::rage::hitbox_expander::size_y = settings::rage::hitbox_expander::size_x;
                    ImAdd::Combo("Type##hbe", &settings::rage::hitbox_expander::target_part,
                        { "Older", "Head", "Torso", "All", "Mixed" });
                    matcha_card_end();
                }
                if (matcha_card("##card_hover", col_w))
                {
                    card_title("Target Hovering");
                    ImAdd::CheckBoxKeyBind("Enabled##orbit", &settings::movement::orbit::enabled, &orbit_key, &orbit_mode);
                    ImAdd::CheckBoxColor("Display Circle##orbit", &orbit_circle, orbit_circle_col);
                    ImAdd::Combo("Target Method##orbit", &settings::movement::orbit::orbit_type, { "Closest To Mouse", "Selected" });
                    ImAdd::SliderFloat("Radius To Mouse##orbit", &settings::movement::orbit::radius, 1.f, 50.f, "%.2f");
                    ImAdd::SliderFloat("Speed##orbit", &settings::movement::orbit::speed, 1.f, 80.f, "%.2f");
                    matcha_card_end();
                }
                if (matcha_card("##card_desync", col_w))
                {
                    card_title("Desync");
                    if (ImAdd::CheckBoxKeyBind("Enabled##desync", &settings::movement::desync::enabled, &desync_key, &settings::movement::desync::activation_mode))
                        settings::movement::desync::keybind = keybind::imgui_key_to_vk(desync_key);
                    ImAdd::CheckBox("Remove Walk Animation##ds", &ds_walk);
                    ImAdd::CheckBox("Display Server Position##ds", &ds_pos);
                    ImAdd::CheckBox("Reset on Desync Off##ds", &ds_reset);
                    ImAdd::CheckBox("Use Tick##ds", &ds_tick);
                    ImAdd::SliderInt("Desync Interval##ds", &ds_interval, 1, 20);
                    matcha_card_end();
                }
                ImGui::EndGroup();
                ImGui::SameLine(0, col_gap);
                ImGui::BeginGroup();
                if (matcha_card("##card_move", col_w))
                {
                    card_title("Movement");
                    if (ImAdd::CheckBoxKeyBind("Anti-Fling##mv", &settings::rage::ghost_mode::enabled, &ghost_key, &settings::rage::ghost_mode::activation_mode))
                        settings::rage::ghost_mode::keybind = keybind::imgui_key_to_vk(ghost_key);
                    ImAdd::CheckBoxKeyBind("No-Clip##mv", &settings::rage::noclip, &noclip_key, &noclip_mode);
                    if (ImAdd::CheckBoxKeyBind("Inf Jump##mv", &settings::movement::bhop::enabled, &bhop_key, &settings::movement::bhop::activation_mode))
                        settings::movement::bhop::keybind = keybind::imgui_key_to_vk(bhop_key);
                    ImAdd::CheckBoxKeyBind("Click TP##mv", &click_tp, &clicktp_key, &clicktp_mode);
                    if (ImAdd::CheckBoxKeyBind("Speed##mv", &settings::movement::speedhack::enabled, &speed_key, &settings::movement::speedhack::activation_mode))
                        settings::movement::speedhack::keybind = keybind::imgui_key_to_vk(speed_key);
                    ImAdd::Combo("Speed Method##mv", &settings::movement::speedhack::mode, { "Velocity", "WalkSpeed" });
                    ImAdd::SliderFloat("Speed Amount##mv", &settings::movement::speedhack::speed, 1.f, 200.f, "%.0f");
                    if (ImAdd::CheckBoxKeyBind("Flight##mv", &settings::movement::flyhack::enabled, &fly_key, &settings::movement::flyhack::activation_mode))
                        settings::movement::flyhack::keybind = keybind::imgui_key_to_vk(fly_key);
                    ImAdd::Combo("Fly Method##mv", &settings::movement::flyhack::mode, { "Velocity", "CFrame" });
                    ImAdd::SliderFloat("Fly Amount##mv", &settings::movement::flyhack::speed, 1.f, 200.f, "%.0f");
                    if (ImAdd::CheckBoxKeyBind("Jump##mv", &settings::movement::jumphack::enabled, &jump_key, &settings::movement::jumphack::activation_mode))
                        settings::movement::jumphack::keybind = keybind::imgui_key_to_vk(jump_key);
                    ImAdd::SliderFloat("Jump Power##mv", &settings::movement::jumphack::value, 1.f, 200.f, "%.0f");
                    ImAdd::CheckBox("Float##mv", &settings::rage::hipheight::enabled);
                    ImAdd::SliderFloat("Height##mv", &settings::rage::hipheight::height, -20.f, 20.f, "%.0f");
                    matcha_card_end();
                }
                ImGui::EndGroup();
                ImGui::PopStyleVar();
            }
            else if (nav_display == 5)  // --- CONFIGS ---
            {
                NavAnim::BeginPanel();
                const float col_gap = 18.f;
                const float avail_w = ImGui::GetContentRegionAvail().x;
                const float page_h = ImGui::GetContentRegionAvail().y;
                const float col_w = ImTrunc((avail_w - col_gap) * 0.5f);
                static char cfg_name[64] = "";
                static int cfg_sel = -1;
                static std::vector<config::config_info_t> cfg_list;
                static bool cfg_refresh = true;
                if (cfg_refresh)
                {
                    cfg_list = config::get_config_list();
                    cfg_refresh = false;
                }

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 10.f));
                const ImVec4 well(26/255.f, 27/255.f, 38/255.f, 1.f); // #1a1b26
                auto dark_well = [&](const char* id, float height)
                {
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, well);
                    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.f);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f, 8.f));
                    ImGui::BeginChild(id, ImVec2(-1.f, height),
                        ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_None);
                };
                auto dark_well_end = []()
                {
                    ImGui::EndChild();
                    ImGui::PopStyleVar(2);
                    ImGui::PopStyleColor(2);
                };

                ImGui::BeginGroup();
                if (matcha_card("##card_scripts", col_w, page_h))
                {
                    card_title("Scripts");
                    if (ImAdd::Button("Open Folder##scr", ImVec2(-1.f, 0.f)))
                        config::open_file_location();
                    ImGui::Text("Script List");
                    dark_well("##scr_list", 220.f);
                    dark_well_end();
                    ImAdd::Button("Load Script##scr", ImVec2(-1.f, 0.f));
                    ImAdd::Button("Set Auto Execute##scr", ImVec2(-1.f, 0.f));
                    matcha_card_end();
                }
                ImGui::EndGroup();
                ImGui::SameLine(0, col_gap);
                ImGui::BeginGroup();
                if (matcha_card("##card_cfg", col_w, page_h))
                {
                    card_title("Config");
                    if (ImAdd::Button("Open Folder##cfg", ImVec2(-1.f, 0.f)))
                        config::open_file_location();
                    ImGui::Text("Config List");
                    dark_well("##cfg_list", 220.f);
                    for (int i = 0; i < (int)cfg_list.size(); ++i)
                    {
                        if (ImGui::Selectable(cfg_list[i].name.c_str(), cfg_sel == i))
                        {
                            cfg_sel = i;
                            strncpy_s(cfg_name, cfg_list[i].name.c_str(), sizeof(cfg_name) - 1);
                        }
                    }
                    dark_well_end();
                    ImGui::Text("Config Name");
                    ImGui::InputText("##cfg_name", cfg_name, IM_ARRAYSIZE(cfg_name));
                    const float bw = (ImGui::GetContentRegionAvail().x - 8.f) * 0.5f;
                    if (ImAdd::Button("Load##cfg", ImVec2(bw, 0.f)))
                    {
                        if (cfg_name[0])
                            config::load_config(cfg_name);
                    }
                    ImGui::SameLine(0, 8.f);
                    if (ImAdd::Button("Save##cfg", ImVec2(bw, 0.f)))
                    {
                        if (cfg_name[0] && config::save_config(cfg_name))
                            cfg_refresh = true;
                    }
                    matcha_card_end();
                }
                ImGui::EndGroup();
                ImGui::PopStyleVar();
            }
            else if (nav_display == 4)  // --- OPTIONS ---
            {
                NavAnim::BeginPanel();
                const float col_gap = 18.f;
                const float avail_w = ImGui::GetContentRegionAvail().x;
                const float col_w = ImTrunc((avail_w - col_gap) * 0.5f);
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 10.f));

                static ImGuiKey menu_key = ImGuiKey_None;
                static ImGuiKey eject_key = ImGuiKey_F10;
                static int eject_mode = 1, present_idx = 0, usage_idx = 1, liquid_idx = 0;
                static bool auto_rescan = false, ingame_input = false, raycast = false, unsafe_lua = false;
                static bool glass_ui = false, eject_on = false, notify_snd = false, music_ov = false;
                static bool keystroke = false, lag_note = false, array_list = false;
                static int fps_cap_ui = 0, raycast_ms = 250, explorer_ms = 500;
                menu_key = keybind::vk_to_imgui_key(settings::menu::menu_keybind);

                ImGui::BeginGroup();
                if (matcha_card("##card_theme", col_w))
                {
                    card_title("Theme");
                    ImGui::Text("Menu Key");
                    const float avail = ImGui::GetContentRegionAvail().x;
                    const ImVec2 row = ImGui::GetCursorScreenPos();
                    ImGui::Dummy(ImVec2(1.f, 22.f));
                    ImGui::SetCursorScreenPos(ImVec2(row.x + avail - 72.f, row.y));
                    if (ImAdd::KeyBind("##menu_key", &menu_key, ImVec2(72.f, 0)))
                        settings::menu::menu_keybind = keybind::imgui_key_to_vk(menu_key);
                    ImGui::SetCursorScreenPos(ImVec2(row.x, row.y + 28.f));
                    ImGui::Text("Accent Color");
                    ImGui::SameLine();
                    ImAdd::ColorEdit4("##accent", settings::menu::accent_color);
                    ImAdd::Combo("Present##opt", &present_idx, { "Tokyo Night" });
                    matcha_card_end();
                }
                if (matcha_card("##card_gen", col_w))
                {
                    card_title("General");
                    ImAdd::CheckBox("Streamproof##opt", &settings::menu::streamproof);
                    ImAdd::CheckBox("Vertical Sync##opt", &settings::menu::vsync);
                    ImAdd::CheckBox("Auto rescan Game##opt", &auto_rescan);
                    ImAdd::CheckBox("In Game Input Detection##opt", &ingame_input);
                    ImAdd::CheckBox("Set Roblox FPS##opt", &settings::cilent::fpscaps::enabled);
                    ImAdd::SliderInt("FPS##opt", &fps_cap_ui, 0, 10000);
                    ImAdd::CheckBox("Raycast Enable##opt", &raycast);
                    ImAdd::SliderInt("Raycast update (ms)##opt", &raycast_ms, 1, 1000);
                    ImAdd::CheckBox("Allow Unsafe LuaU##opt", &unsafe_lua);
                    ImAdd::Combo("Usage##opt", &usage_idx, { "Low", "Mid", "High" });
                    matcha_card_end();
                }
                if (matcha_card("##card_ui", col_w))
                {
                    card_title("User Interface");
                    bool dark_bg = settings::menu::bg_opacity > 0.05f;
                    if (ImAdd::CheckBox("Dark Background##opt", &dark_bg))
                        settings::menu::bg_opacity = dark_bg ? 0.90f : 0.f;
                    ImAdd::CheckBox("Snow Effect##opt", &settings::menu::rain_effect);
                    ImAdd::CheckBox("Blur##opt", &settings::lighting::blur::enabled);
                    ImAdd::CheckBox("Glass##opt", &glass_ui);
                    matcha_card_end();
                }
                ImGui::EndGroup();
                ImGui::SameLine(0, col_gap);
                ImGui::BeginGroup();
                if (matcha_card("##card_sess", col_w))
                {
                    card_title("Session");
                    auto session_pill = [](const char* label)
                    {
                        ImDrawList* dl = ImGui::GetWindowDrawList();
                        ImFont* f = MenuTheme::Fonts::Bold ? MenuTheme::Fonts::Bold
                            : (MenuTheme::Fonts::SemiBold ? MenuTheme::Fonts::SemiBold : ImGui::GetFont());
                        const float fsz = MenuTheme::FontPx(13.f);
                        const ImVec2 tsz = f->CalcTextSizeA(fsz, FLT_MAX, 0.f, label);
                        const float pad_x = 10.f;
                        const float pad_y = 4.f;
                        const float avail = ImGui::GetContentRegionAvail().x;
                        const ImVec2 cur = ImGui::GetCursorScreenPos();
                        const float x = IM_ROUND(cur.x + (avail - tsz.x) * 0.5f);
                        const float y = IM_ROUND(cur.y + pad_y);
                        const ImVec2 o0(IM_ROUND(x - pad_x), IM_ROUND(y - pad_y));
                        const ImVec2 o1(IM_ROUND(x + tsz.x + pad_x), IM_ROUND(y + tsz.y + pad_y));
                        const float r = IM_ROUND(ImMin((o1.y - o0.y) * 0.5f, pad_x));
                        dl->AddRect(o0, o1, IM_COL32(0, 0, 0, 255), r, 0, 1.25f);
                        dl->AddText(f, fsz, ImVec2(x, y), IM_COL32(0, 0, 0, 255), label);
                        ImGui::Dummy(ImVec2(1.f, tsz.y + pad_y * 2.f + 6.f));
                    };
                    {
                        ImFont* f = MenuTheme::Fonts::SemiBold ? MenuTheme::Fonts::SemiBold : ImGui::GetFont();
                        const float fsz = MenuTheme::FontPx(13.f);
                        const char* det = "Dejected";
                        const ImVec2 tsz = f->CalcTextSizeA(fsz, FLT_MAX, 0.f, det);
                        const float avail = ImGui::GetContentRegionAvail().x;
                        const ImVec2 cur = ImGui::GetCursorScreenPos();
                        ImGui::GetWindowDrawList()->AddText(f, fsz,
                            ImVec2(IM_ROUND(cur.x + (avail - tsz.x) * 0.5f), IM_ROUND(cur.y)),
                            IM_COL32(122, 131, 165, 255), det);
                        ImGui::Dummy(ImVec2(1.f, tsz.y + 6.f));
                    }
                    session_pill("Standard");
                    const std::uint64_t gid = game::datamodel.address
                        ? memory->read<std::uint64_t>(game::datamodel.address + OFFSET(DataModel, GameId)) : 0ULL;
                    const std::uint64_t pid = game::datamodel.address
                        ? memory->read<std::uint64_t>(game::datamodel.address + OFFSET(DataModel, PlaceId)) : 0ULL;
                    char gid_s[32], pid_s[32];
                    snprintf(gid_s, sizeof(gid_s), "%llu", (unsigned long long)gid);
                    snprintf(pid_s, sizeof(pid_s), "%llu", (unsigned long long)pid);
                    auto id_row = [](const char* label, const char* value, const char* copy_id, bool value_black)
                    {
                        const float avail = ImGui::GetContentRegionAvail().x;
                        const ImVec2 row = ImGui::GetCursorScreenPos();
                        const float h = ImGui::GetFrameHeight();
                        ImGui::TextUnformatted(label);
                        const float copy_w = 56.f;
                        const ImVec2 vs = ImGui::CalcTextSize(value);
                        ImGui::SetCursorScreenPos(ImVec2(row.x + avail - copy_w, row.y));
                        if (ImAdd::Button(copy_id, ImVec2(copy_w, 0.f)))
                            ImGui::SetClipboardText(value);
                        ImGui::SetCursorScreenPos(ImVec2(row.x + avail - copy_w - 8.f - vs.x,
                            row.y + (h - vs.y) * 0.5f));
                        if (value_black)
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.f, 0.f, 1.f));
                        ImGui::TextUnformatted(value);
                        if (value_black)
                            ImGui::PopStyleColor();
                        ImGui::SetCursorScreenPos(ImVec2(row.x, row.y + h + ImGui::GetStyle().ItemSpacing.y));
                    };
                    id_row("Game ID", gid_s, "Copy##gid", false);
                    id_row("Place ID", pid_s, "Copy##pid", true);
                    const float bw = (ImGui::GetContentRegionAvail().x - 8.f) * 0.5f;
                    ImAdd::Button("Rejoin##opt", ImVec2(bw, 0.f));
                    ImGui::SameLine(0, 8.f);
                    ImAdd::Button("Server Hop##opt", ImVec2(bw, 0.f));
                    if (ImAdd::Button("Rescan##opt", ImVec2(-1.f, 0.f)))
                        ForceRescan();
                    ImAdd::Button("Eject##opt", ImVec2(-1.f, 0.f));
                    ImAdd::CheckBoxKeyBind("Eject Bind##opt", &eject_on, &eject_key, &eject_mode);
                    matcha_card_end();
                }
                if (matcha_card("##card_uie", col_w))
                {
                    card_title("UI Elements");
                    ImAdd::CheckBox("Watermark##uie", &settings::menu::watermark);
                    ImAdd::CheckBox("View Explorer##uie", &MenuTheme::show_explorer);
                    ImAdd::SliderInt("Explorer update (ms)##uie", &explorer_ms, 50, 2000);
                    ImAdd::CheckBox("Keybind List##uie", &settings::ui::keybinds);
                    ImAdd::CheckBox("Notification Sound##uie", &notify_snd);
                    ImAdd::CheckBox("Music Overlay##uie", &music_ov);
                    ImAdd::Combo("Liquid Background Type##uie", &liquid_idx, { "Lava" });
                    ImAdd::CheckBox("Keystroke##uie", &keystroke);
                    ImAdd::CheckBox("Lag Notifier##uie", &lag_note);
                    ImAdd::CheckBox("Array List##uie", &array_list);
                    matcha_card_end();
                }
                ImGui::EndGroup();
                ImGui::PopStyleVar();
            }
            else if (nav_display == 6)  // --- NPC ---
            {
                NavAnim::BeginPanel();
                const float col_gap = 18.f;
                const float avail_w = ImGui::GetContentRegionAvail().x;
                const float page_h = ImGui::GetContentRegionAvail().y;
                const float col_w = ImTrunc((avail_w - col_gap) * 0.5f);
                static char npc_cfg[64] = "";
                static int npc_sel = -1;
                static int npc_sub = 0;
                static int npc_cfg_sel = -1;
                static std::vector<config::config_info_t> npc_cfgs;
                static bool npc_cfg_refresh = true;
                if (npc_cfg_refresh)
                {
                    npc_cfgs = config::get_config_list();
                    npc_cfg_refresh = false;
                }

                std::vector<settings::custom_entities::custom_container_t> npc_snap;
                {
                    std::lock_guard<std::mutex> lock(custom_entities::containers_mtx);
                    npc_snap = settings::custom_entities::containers;
                }

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 10.f));
                ImGui::BeginGroup();
                if (matcha_card("##card_npclist", col_w, page_h))
                {
                    card_title("NPC List");
                    const float bw = (ImGui::GetContentRegionAvail().x - 8.f) * 0.5f;
                    if (ImAdd::Button("+ Model##npc", ImVec2(bw, 0.f)))
                    {
                        const char* path = npc_cfg[0] ? npc_cfg : settings::custom_entities::current_input.c_str();
                        custom_entities::add_container(path);
                        settings::custom_entities::show_custom_entities = true;
                    }
                    ImGui::SameLine(0, 8.f);
                    if (ImAdd::Button("+ Directory##npc", ImVec2(bw, 0.f)))
                    {
                        const char* path = npc_cfg[0] ? npc_cfg : settings::custom_entities::current_input.c_str();
                        custom_entities::add_container(path);
                        settings::custom_entities::show_custom_entities = true;
                    }
                    const float list_h = ImMax(40.f, ImGui::GetContentRegionAvail().y);
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(26/255.f, 27/255.f, 38/255.f, 1.f));
                    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.f);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f, 8.f));
                    ImGui::BeginChild("##npc_list", ImVec2(-1.f, list_h),
                        ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_None);
                    int row = 0;
                    for (const auto& cont : npc_snap)
                    {
                        ImGui::PushID(cont.path.c_str());
                        if (ImGui::Selectable(cont.name.c_str(), npc_sel == row))
                            npc_sel = row;
                        ++row;
                        for (const auto& ent : cont.entities)
                        {
                            ImGui::PushID((int)ent.instance.address);
                            if (ImGui::Selectable(ent.name.c_str(), npc_sel == row))
                                npc_sel = row;
                            ImGui::PopID();
                            ++row;
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleVar(2);
                    ImGui::PopStyleColor(2);
                    matcha_card_end();
                }
                ImGui::EndGroup();
                ImGui::SameLine(0, col_gap);
                ImGui::BeginGroup();
                if (matcha_card("##card_npcedit", col_w))
                {
                    const char* ntabs[] = { "General", "Visuals" };
                    inner_tabs(ntabs, 2, &npc_sub);
                    ImGui::Dummy(ImVec2(1.f, 18.f));
                    const char* empty = "Select an NPC to edit";
                    const ImVec2 ts = ImGui::CalcTextSize(empty);
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImMax(0.f, (ImGui::GetContentRegionAvail().x - ts.x) * 0.5f));
                    ImGui::TextDisabled("%s", empty);
                    ImGui::Dummy(ImVec2(1.f, 18.f));
                    if (npc_sel >= 0)
                    {
                        if (npc_sub == 0)
                            ImAdd::CheckBox("Enabled##npc_en", &settings::custom_entities::show_custom_entities);
                        else
                            ImAdd::CheckBox("Show ESP##npc_vis", &settings::custom_entities::show_custom_entities);
                    }
                    matcha_card_end();
                }
                if (matcha_card("##card_npccfg", col_w))
                {
                    card_title("Config");
                    ImGui::InputText("##npc_cfg_name", npc_cfg, IM_ARRAYSIZE(npc_cfg));
                    const float tw = (ImGui::GetContentRegionAvail().x - 16.f) / 3.f;
                    if (ImAdd::Button("Load##npc_cfg", ImVec2(tw, 0.f)))
                    {
                        if (npc_cfg[0])
                            config::load_config(npc_cfg);
                    }
                    ImGui::SameLine(0, 8.f);
                    if (ImAdd::Button("Save##npc_cfg", ImVec2(tw, 0.f)))
                    {
                        if (npc_cfg[0] && config::save_config(npc_cfg))
                            npc_cfg_refresh = true;
                    }
                    ImGui::SameLine(0, 8.f);
                    if (ImAdd::Button("Folder##npc_cfg", ImVec2(tw, 0.f)))
                        config::open_file_location();
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(26/255.f, 27/255.f, 38/255.f, 1.f));
                    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.f);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f, 8.f));
                    ImGui::BeginChild("##npc_cfg_list", ImVec2(-1.f, 96.f),
                        ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_None);
                    for (int i = 0; i < (int)npc_cfgs.size(); ++i)
                    {
                        if (ImGui::Selectable(npc_cfgs[i].name.c_str(), npc_cfg_sel == i))
                        {
                            npc_cfg_sel = i;
                            strncpy_s(npc_cfg, npc_cfgs[i].name.c_str(), sizeof(npc_cfg) - 1);
                        }
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleVar(2);
                    ImGui::PopStyleColor(2);
                    matcha_card_end();
                }
                ImGui::EndGroup();
                ImGui::PopStyleVar();
            }
            else if (nav_display == 7)  // --- TEAMS ---
            {
                NavAnim::BeginPanel();
                const float col_gap = 18.f;
                const float avail_w = ImGui::GetContentRegionAvail().x;
                const float page_h = ImGui::GetContentRegionAvail().y;
                const float col_w = ImTrunc((avail_w - col_gap) * 0.5f);
                ImGui::BeginGroup();
                if (matcha_card("##card_teams", col_w, page_h))
                {
                    card_title("Teams");
                    static char team_search[64] = "";
                    static bool custom_teams = false, auto_ally = false;
                    ImGui::InputTextWithHint("##team_search", "Search team...", team_search, IM_ARRAYSIZE(team_search));
                    ImAdd::CheckBox("Use Custom Teams##tm", &custom_teams);
                    ImAdd::CheckBox("Auto Detect Allied Team##tm", &auto_ally);
                    ImGui::TextDisabled("Team");
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 40.f);
                    ImGui::TextDisabled("Status");
                    const float list_h = ImMax(40.f, ImGui::GetContentRegionAvail().y);
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(26/255.f, 27/255.f, 38/255.f, 1.f));
                    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.f);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f, 8.f));
                    ImGui::BeginChild("##team_list", ImVec2(-1.f, list_h),
                        ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_None);
                    ImGui::TextDisabled("No teams in this game");
                    ImGui::EndChild();
                    ImGui::PopStyleVar(2);
                    ImGui::PopStyleColor(2);
                    matcha_card_end();
                }
                ImGui::EndGroup();
                ImGui::SameLine(0, col_gap);
                ImGui::BeginGroup();
                if (matcha_card("##card_tedit", col_w))
                {
                    card_title("Editor");
                    ImGui::TextDisabled("Select a team to edit");
                    matcha_card_end();
                }
                if (matcha_card("##card_tqa", col_w))
                {
                    card_title("Quick Actions");
                    const float bw = (ImGui::GetContentRegionAvail().x - 8.f) * 0.5f;
                    ImAdd::Button("All Enemy##tm", ImVec2(bw, 0.f));
                    ImGui::SameLine(0, 8.f);
                    ImAdd::Button("All Ally##tm", ImVec2(bw, 0.f));
                    ImAdd::Button("Reset to Default##tm", ImVec2(-1.f, 0.f));
                    matcha_card_end();
                }
                ImGui::EndGroup();
            }
            ImGui::PopStyleVar(); // ItemSpacing inside body
            ImAdd::EndChild();
            MenuTheme::EndContentFit();
        }
        else
        {
            MenuTheme::EndContentFit();
            ImGui::EndChild();
        }

        // Players panel is an independent overlay â€” always disable playerlist when not shown
        if (!MenuTheme::show_players)
            settings::rage::playerlist::enabled = false;
    }
    else
    {
        m_bMainWindowOpen = true;
        m_bMenuVisible = true;
    }
    // Begin() always needs a matching End(), even when it returns false.
    // Skipping End() corrupts ImGui's window stack and the menu stops drawing.
    MenuTheme::EndMainWindow();

    // Top icon bar — floating pill (icons removed; hit zones stay)
    {
        ImDrawList* fdl = ImGui::GetForegroundDrawList();
        const int vtx0 = fdl->VtxBuffer.Size;
        const int cmd0 = fdl->CmdBuffer.Size;
        MenuTheme::DrawTopIconBar(&nav_page);
        const float eased = OpenCloseAnim::Ease();
        const float scale = ImLerp(0.92f, 1.f, eased);
        const float y_shift = (1.f - eased) * 16.f;
        if (scale < 0.9995f || y_shift >= 0.05f)
        {
            ImGuiWindow* main = ImGui::FindWindowByName("##MatchaMain");
            const ImVec2 origin = main
                ? ImVec2(main->Pos.x + main->Size.x * 0.5f, main->Pos.y + main->Size.y * 0.5f)
                : ImGui::GetIO().DisplaySize * 0.5f;
            for (int i = vtx0; i < fdl->VtxBuffer.Size; ++i)
            {
                fdl->VtxBuffer[i].pos.x = origin.x + (fdl->VtxBuffer[i].pos.x - origin.x) * scale;
                fdl->VtxBuffer[i].pos.y = origin.y + (fdl->VtxBuffer[i].pos.y - origin.y) * scale + y_shift;
            }
            for (int i = cmd0; i < fdl->CmdBuffer.Size; ++i)
            {
                ImVec4& cr = fdl->CmdBuffer[i].ClipRect;
                cr.x = origin.x + (cr.x - origin.x) * scale;
                cr.y = origin.y + (cr.y - origin.y) * scale + y_shift;
                cr.z = origin.x + (cr.z - origin.x) * scale;
                cr.w = origin.y + (cr.w - origin.y) * scale + y_shift;
            }
        }
    }

    EnsureUiIconsLoaded();
#if 0
    if (false)
    {
        ImGuiWindow* mw2 = ImGui::FindWindowByName("##MatchaMain");
        const float gap2  = 6.f;
        const float pw2   = 340.f;
        const float pad   = 14.f;

        // Uniform inset around cover art — top/side gaps match.
        const float gap      = 18.f;
        const float art_sz   = pw2 - gap * 2.f;
        const float art_gap  = 10.f;
        const float art_round = 6.f;
        const float below_h  = 158.f;
        const float ph2      = gap + art_sz + art_gap + below_h;

        const ImVec2 mpos2 = mw2 ? mw2->Pos : ImVec2(io.DisplaySize.x * 0.5f - MenuTheme::GetWindowW() * 0.5f, io.DisplaySize.y * 0.5f - MenuTheme::GetWindowH() * 0.5f);
        const float menu_w2 = mw2 ? mw2->Size.x : MenuTheme::GetWindowW();
        const float px2 = mpos2.x + menu_w2 + gap2;
        const float py2 = mpos2.y;
        ImVec2 sp_sz(pw2, ph2);
        if (ImGuiWindow* existing = ImGui::FindWindowByName("##SpotifyIsland"))
            sp_sz = existing->Size;
        ImGui::SetNextWindowPos(MenuTheme::TickSmoothDrag("##SpotifyIsland", sp_sz, ImVec2(px2, py2), ImMax(40.f, sp_sz.y - below_h), 0.f, 0.f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(pw2, ph2), ImGuiCond_Once);
        ImGui::SetNextWindowSizeConstraints(ImVec2(220.f, 280.f), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        bool sp_win = ImGui::Begin("##SpotifyIsland", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove);
        ImGui::PopStyleVar(2);

        if (sp_win)
        {
            ImRect      sp_bb = ImGui::GetCurrentWindow()->Rect();
            ImDrawList* sp_dl = ImGui::GetWindowDrawList();
            ImFont*     fnt   = MenuTheme::Fonts::SemiBold ? MenuTheme::Fonts::SemiBold : ImGui::GetFont();
            const float fsz   = MenuTheme::Sz(MenuTheme::Typography::kLabelSize);
            const ImVec2 wt   = sp_bb.Min;
            const float  R    = 17.f; // match overlay spotify widget shell rounding
            const float  win_w = sp_bb.GetWidth();
            const float  win_h = sp_bb.GetHeight();
            float art_sz = win_w - gap * 2.f;
            const float max_art = win_h - gap - art_gap - below_h;
            if (art_sz > max_art) art_sz = max_art;
            if (art_sz < 72.f) art_sz = 72.f;

            // Panel shell — match satellite panel opacity (same as ESP Preview, Players)
            if (sp_playing)
            {
                float eq[10]{};
                audio_eq::get_levels(eq);
                MenuTheme::DrawWindowGlowReactive(wt, sp_bb.Max, R, eq);
            }
            else
            {
                MenuTheme::DrawWindowGlow(wt, sp_bb.Max, R);
            }
            backdrop_blur::fill(sp_dl, wt, sp_bb.Max, R, IM_COL32(37, 37, 46, 250));

            spotify_now::Track sp_tr = spotify_now::get();

            // Album art — square inside the original slot, same UV crop + rounding as overlay
            const ImVec2 at(wt.x + gap, wt.y + gap);
            const ImVec2 ab(at.x + art_sz, at.y + art_sz);
            ID3D11ShaderResourceView* art_srv = sp_get_album_art_srv();
            if (art_srv)
            {
                sp_dl->AddImageRounded(
                    (ImTextureID)art_srv, at, ab,
                    ImVec2(0.09f, 0.f), ImVec2(0.91f, 0.82f),
                    IM_COL32(255, 255, 255, 255), art_round);
            }
            else
            {
                sp_dl->AddRectFilled(at, ab, IM_COL32(26, 26, 30, 255), art_round, ImDrawFlags_RoundCornersAll);
                // Play-triangle placeholder
                float isz = art_sz * 0.18f;
                ImVec2 cc(at.x + art_sz * 0.5f, at.y + art_sz * 0.5f);
                sp_dl->AddTriangleFilled(
                    ImVec2(cc.x - isz*0.35f, cc.y - isz),
                    ImVec2(cc.x + isz*0.65f, cc.y),
                    ImVec2(cc.x - isz*0.35f, cc.y + isz),
                    IM_COL32(29,185,84,128));
            }

            // Content below art
            const float tx      = gap + pad * 0.5f;
            float       cur_y   = ab.y + art_gap;

            if (!sp_tr.title.empty())
            {
                // Clamp helper
                auto clamp_str = [&](const std::string& s, float sz_f, float max_w) -> std::string {
                    if (fnt->CalcTextSizeA(sz_f, FLT_MAX, 0.f, s.c_str()).x <= max_w) return s;
                    std::string r = s;
                    while (!r.empty() &&
                        fnt->CalcTextSizeA(sz_f, FLT_MAX, 0.f, (r+"..").c_str()).x > max_w)
                        r.pop_back();
                    return r + "..";
                };
                const float text_w = win_w - tx * 2.f;

                ImFont* tf      = fnt;
                const float tsz = fsz;
                std::string title_d = clamp_str(sp_tr.title, tsz, text_w);
                float title_w = tf->CalcTextSizeA(tsz, FLT_MAX, 0.f, title_d.c_str()).x;
                sp_dl->AddText(tf, tsz,
                    ImVec2(wt.x + (win_w - title_w) * 0.5f, cur_y),
                    IM_COL32(235,235,242,255), title_d.c_str());
                cur_y += tsz + 4.f;

                ImFont* af      = fnt;
                const float asz = MenuTheme::Sz(12.f);
                std::string artist_d = clamp_str(sp_tr.artist, asz, text_w);
                float artist_w = af->CalcTextSizeA(asz, FLT_MAX, 0.f, artist_d.c_str()).x;
                sp_dl->AddText(af, asz,
                    ImVec2(wt.x + (win_w - artist_w) * 0.5f, cur_y),
                    IM_COL32(133,141,149,210), artist_d.c_str());
                cur_y += asz + 16.f;

                // ── Progress bar — gelato smooth_ms logic ─────────────────────
                const float pb_x = wt.x + tx;
                const float pb_w = win_w - tx * 2.f;
                const float pb_h = 3.f;
                // Taller invisible hit area so the bar is easy to click/drag
                const float pb_hit = 12.f;
                const float pb_hit_y = cur_y - (pb_hit - pb_h) * 0.5f;

                // Invisible button covering the bar for interaction
                ImGui::SetCursorScreenPos(ImVec2(pb_x, pb_hit_y));
                ImGui::InvisibleButton("##sp_seek", ImVec2(pb_w, pb_hit));
                const bool pb_hov     = ImGui::IsItemHovered();
                const bool pb_held    = ImGui::IsItemActive();
                const bool pb_clicked = ImGui::IsItemClicked();

                // While dragging or just clicked, compute new position from mouse
                static bool s_seeking = false;
                static float s_seek_prog = 0.f;
                if (pb_held) {
                    s_seeking  = true;
                    s_seek_prog = ImClamp((ImGui::GetIO().MousePos.x - pb_x) / pb_w, 0.f, 1.f);
                } else if (s_seeking) {
                    // Released — send the seek command
                    sp_cmd_seek((int64_t)(s_seek_prog * (float)sp_duration_ms));
                    s_seeking = false;
                }

                // Display progress: use drag position while seeking, smooth_ms otherwise
                const float prog = s_seeking ? s_seek_prog
                    : (sp_duration_ms > 0 ? ImClamp(sp_smooth_ms / (float)sp_duration_ms, 0.f, 1.f) : 0.f);

                // Grow bar height slightly when hovered/held
                const float pb_h_draw = (pb_hov || pb_held) ? pb_h * 1.6f : pb_h;
                const float pb_y_draw = cur_y + (pb_h - pb_h_draw) * 0.5f;

                // Track background
                sp_dl->AddRectFilled(
                    ImVec2(pb_x, pb_y_draw), ImVec2(pb_x + pb_w, pb_y_draw + pb_h_draw),
                    IM_COL32(32,32,36,153), pb_h_draw * 0.5f);
                // Fill
                if (prog > 0.005f)
                {
                    float fw = pb_w * prog;
                    sp_dl->AddRectFilled(
                        ImVec2(pb_x, pb_y_draw), ImVec2(pb_x + fw, pb_y_draw + pb_h_draw),
                        IM_COL32(255,255,255,255), pb_h_draw * 0.5f);
                    // Thumb dot — larger when hovered/held
                    if (fw > pb_h * 3)
                    {
                        float dx = pb_x + fw, dy = pb_y_draw + pb_h_draw * 0.5f;
                        float dr = (pb_hov || pb_held) ? pb_h_draw * 1.8f : pb_h_draw * 1.4f;
                        sp_dl->AddCircleFilled(ImVec2(dx, dy), dr, IM_COL32(255,255,255,255));
                    }
                }
                cur_y += pb_h + 4.f;

                // Time label — uses smooth_ms so it ticks every frame, not every poll
                {
                    int pm = (int)(sp_smooth_ms / 1000.f);
                    char tbuf[16]; snprintf(tbuf, sizeof(tbuf), "%d:%02d", pm/60, pm%60);
                    sp_dl->AddText(fnt, fsz * 0.78f, ImVec2(pb_x, cur_y),
                        IM_COL32(102,110,115,153), tbuf);
                    // Duration (right-aligned)
                    int dm = (int)(sp_duration_ms / 1000);
                    char dbuf[16]; snprintf(dbuf, sizeof(dbuf), "%d:%02d", dm/60, dm%60);
                    float dw = fnt->CalcTextSizeA(fsz * 0.78f, FLT_MAX, 0.f, dbuf).x;
                    sp_dl->AddText(fnt, fsz * 0.78f, ImVec2(pb_x + pb_w - dw, cur_y),
                        IM_COL32(102,110,115,153), dbuf);
                }
                cur_y += fsz * 0.78f + 6.f;

                // ── Transport controls (prev / play-pause / next) ─────────────
                EnsureSpLogoLoaded();
                EnsureSpIconsLoaded();

                const float btn_s      = 22.f;
                const float btn_gap    = pad;
                const float btns_w    = btn_s * 3.f + btn_gap * 2.f;
                const float btn_start_x = wt.x + (win_w - btns_w) * 0.5f;

                // Helper: invisible hit area + image icon tinted white (dimmer when not hovered)
                auto draw_icon_btn = [&](const char* id, ImVec2 pos, ID3D11ShaderResourceView* srv) -> bool {
                    ImGui::SetCursorScreenPos(pos);
                    ImGui::InvisibleButton(id, ImVec2(btn_s, btn_s));
                    bool hov     = ImGui::IsItemHovered();
                    bool clicked = ImGui::IsItemClicked();
                    ImU32 tint   = hov ? IM_COL32(255,255,255,255) : IM_COL32(255,255,255,140);
                    if (srv)
                        sp_dl->AddImage((ImTextureID)srv, pos, ImVec2(pos.x + btn_s, pos.y + btn_s),
                            ImVec2(0,0), ImVec2(1,1), tint);
                    return clicked;
                };

                // Prev
                if (draw_icon_btn("##sp_prev", ImVec2(btn_start_x, cur_y), g_sp_icon_prev))
                    sp_cmd_prev();

                // Play / Pause
                auto* play_pause_srv = sp_playing ? g_sp_icon_pause : g_sp_icon_play;
                if (draw_icon_btn("##sp_play", ImVec2(btn_start_x + btn_s + btn_gap, cur_y), play_pause_srv))
                    sp_cmd_playpause();

                // Next
                if (draw_icon_btn("##sp_next", ImVec2(btn_start_x + btn_s * 2.f + btn_gap * 2.f, cur_y), g_sp_icon_next))
                    sp_cmd_next();

                cur_y += btn_s + 12.f;
                {
                    const float vol_icon = 22.f;
                    const float vol_h = 5.f;
                    const float vol_hit = 22.f;
                    const float row_x = wt.x + pad;
                    const float row_w = win_w - pad * 2.f;
                    const float cluster_w = row_w * 0.62f;
                    const float cluster_x = row_x + (row_w - cluster_w) * 0.5f;
                    const ImVec2 mute_p(cluster_x, cur_y + (vol_hit - vol_icon) * 0.5f);
                    const ImVec2 vol_p(cluster_x + cluster_w - vol_icon, cur_y + (vol_hit - vol_icon) * 0.5f);

                    ImGui::SetCursorScreenPos(mute_p);
                    ImGui::InvisibleButton("##sp_mute", ImVec2(vol_icon, vol_icon));
                    const bool mute_hov = ImGui::IsItemHovered();
                    if (ImGui::IsItemClicked())
                        audio_eq::set_muted(true);
                    if (g_sp_icon_volume_mute)
                        sp_dl->AddImage((ImTextureID)g_sp_icon_volume_mute, mute_p,
                            ImVec2(mute_p.x + vol_icon, mute_p.y + vol_icon),
                            ImVec2(0, 0), ImVec2(1, 1),
                            mute_hov ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 255, 255, 140));

                    ImGui::SetCursorScreenPos(vol_p);
                    ImGui::InvisibleButton("##sp_vol_icon", ImVec2(vol_icon, vol_icon));
                    const bool vol_hov = ImGui::IsItemHovered();
                    if (ImGui::IsItemClicked())
                        audio_eq::set_muted(false);
                    if (g_sp_icon_volume)
                        sp_dl->AddImage((ImTextureID)g_sp_icon_volume, vol_p,
                            ImVec2(vol_p.x + vol_icon, vol_p.y + vol_icon),
                            ImVec2(0, 0), ImVec2(1, 1),
                            vol_hov ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 255, 255, 140));

                    const float sl_x = mute_p.x + vol_icon + 8.f;
                    const float sl_w = vol_p.x - 8.f - sl_x;
                    ImGui::SetCursorScreenPos(ImVec2(sl_x, cur_y));
                    ImGui::InvisibleButton("##sp_vol", ImVec2(sl_w, vol_hit));
                    const bool vh = ImGui::IsItemHovered();
                    const bool vd = ImGui::IsItemActive();
                    static bool  s_vol_drag = false;
                    static float s_vol_ui = -1.f;
                    if (s_vol_ui < 0.f)
                        s_vol_ui = audio_eq::get_volume();
                    if (vd)
                    {
                        s_vol_drag = true;
                        s_vol_ui = ImClamp((ImGui::GetIO().MousePos.x - sl_x) / sl_w, 0.f, 1.f);
                        audio_eq::set_volume(s_vol_ui);
                    }
                    else if (s_vol_drag)
                    {
                        s_vol_drag = false;
                    }
                    else if (!audio_eq::get_muted())
                    {
                        s_vol_ui = audio_eq::get_volume();
                    }

                    const float shown = audio_eq::get_muted() ? 0.f : s_vol_ui;
                    const float bar_h = (vh || vd) ? vol_h * 1.45f : vol_h;
                    const float bar_y = cur_y + (vol_hit - bar_h) * 0.5f;
                    sp_dl->AddRectFilled(ImVec2(sl_x, bar_y), ImVec2(sl_x + sl_w, bar_y + bar_h),
                        IM_COL32(255, 255, 255, 40), bar_h * 0.5f);
                    if (shown > 0.005f)
                    {
                        sp_dl->AddRectFilled(ImVec2(sl_x, bar_y), ImVec2(sl_x + sl_w * shown, bar_y + bar_h),
                            IM_COL32(255, 255, 255, 230), bar_h * 0.5f);
                    }
                }
            }
        }
        ImGui::End();
    }
#endif

    // -----------------------------------------------------------------------
    // Theme customization panel -- drops down below the pencil button,
    // sits above (in front of) the main menu.
    // -----------------------------------------------------------------------
    if (MenuTheme::show_theme)
    {
        constexpr float kThemeW = 320.f;

        // Mirror the top icon bar layout constants from DrawTopIconBar
        constexpr float kBtnSz  = 36.f;
        constexpr float kGap    = 12.f;
        constexpr float kPadH   = 14.f;
        constexpr float kPadV   = 8.f;
        constexpr int   kCount  = 4;
        const float bar_w = kPadH * 2.f + kCount * kBtnSz + (kCount - 1) * kGap;
        const float bar_h = kPadV * 2.f + kBtnSz;
        const float bar_gap = 6.f;

        const ImVec2 display_th = ImGui::GetIO().DisplaySize;
        const float bar_x = ImFloor(display_th.x * 0.5f - bar_w * 0.5f);
        const float bar_y = 10.f;

        // Pencil button (index 3) centre x
        const float pencil_cx = bar_x + kPadH + 3 * (kBtnSz + kGap) + kBtnSz * 0.5f;

        // Panel top: just below the pill bar
        float th_x = ImFloor(pencil_cx - kThemeW * 0.5f);
        th_x = ImClamp(th_x, 4.f, display_th.x - kThemeW - 4.f);
        const float th_y = bar_y + bar_h + bar_gap;

        // Panel bottom: just above the top of the main menu (Favorites/Standard row)
        // Menu is centred at (display.y*0.5 - 60) with kWindowH height
        const float menu_top = ImFloor(display_th.y * 0.5f - 60.f - MenuTheme::GetWindowH() * 0.5f);
        const float kThemeH = ImMax(40.f, menu_top - th_y - bar_gap);

        ImGui::SetNextWindowPos(ImVec2(th_x, th_y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(kThemeW, kThemeH), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        bool th_win = ImGui::Begin("##ThemePanel", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::PopStyleVar(2);

        if (th_win)
        {
            ImRect th_bb(ImGui::GetCurrentWindow()->Rect());

            // Plain dark panel â€” no titlebar/chrome
            ImDrawList* th_dl = ImGui::GetWindowDrawList();
            MenuTheme::DrawWindowGlow(th_bb.Min, th_bb.Max, 8.f);
            backdrop_blur::fill(th_dl, th_bb.Min, th_bb.Max, 16.f, IM_COL32(37, 37, 46, 250));
            th_dl->AddRect(th_bb.Min, th_bb.Max, MenuTheme::ColBorder(), 8.f, 0, 1.f);

            const float pad = 12.f;
            ImGui::SetCursorScreenPos(th_bb.Min + ImVec2(pad, pad));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, style.WindowPadding);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildPadding, style.WindowPadding);
            bool th_body = ImGui::BeginChild("th_body",
                ImVec2(kThemeW - pad * 2.f, kThemeH - pad * 2.f),
                ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
            ImGui::PopStyleVar(2);

            if (th_body)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.f, 7.f));

                // ── ACCENT COLOR ─────────────────────────────────────────────
                NAV_ROW { MenuTheme::SectionHeader("ACCENT COLOR"); }
                ImGui::Text("Color");
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImAdd::GetColorPickerWidth() * 3.f - style.ItemSpacing.x * 2.f + style.ChildPadding.x);
                NAV_ROW { ImAdd::ColorEdit4("##accent_col", settings::menu::accent_color); }

                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                // ── MENU PANEL COLORS ─────────────────────────────────────────
                NAV_ROW { MenuTheme::SectionHeader("MENU COLORS"); }

                auto CR = [&](const char* label, float* col4) {
                    ImGui::Text("%s", label);
                    ImGui::SameLine(ImGui::CalcItemWidth() - ImAdd::GetColorPickerWidth());
                    char id[40]; snprintf(id, sizeof(id), "##mc_%s", label);
                    NAV_ROW { ImAdd::ColorEdit4(id, col4); }
                };

                CR("Sidebar",     settings::menu::color_sidebar);
                CR("Header",      settings::menu::color_header);
                CR("Content",     settings::menu::color_content);
                CR("Topbar",      settings::menu::color_topbar);
                CR("Border",      settings::menu::color_border);
                CR("Child Panel", settings::menu::color_child);
                CR("Text",        settings::menu::color_text);
                CR("Text Muted",  settings::menu::color_text_muted);

                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                // ── BACKGROUND ───────────────────────────────────────────────
                NAV_ROW { MenuTheme::SectionHeader("BACKGROUND"); }
                NAV_ROW { ImAdd::SliderFloat("Opacity##bg", &settings::menu::bg_opacity, 0.f, 1.f, "%.2f"); }

                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                // ── RAIN EFFECT ──────────────────────────────────────────────
                NAV_ROW { MenuTheme::SectionHeader("RAIN EFFECT"); }
                NAV_ROW { ImAdd::CheckBox("Enable##rain", &settings::menu::rain_effect); }
                if (settings::menu::rain_effect)
                {
                    NAV_ROW { ImAdd::SliderFloat("Opacity##rain", &settings::menu::rain_opacity, 0.f, 1.f, "%.2f"); }
                    NAV_ROW { ImAdd::SliderInt("Count##rain",  &settings::menu::rain_count, 0, 120, "%d"); }
                }

                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                // ── LAYOUT ───────────────────────────────────────────────────
                NAV_ROW { MenuTheme::SectionHeader("LAYOUT"); }
                NAV_ROW { ImAdd::SliderFloat("Menu Scale",    &settings::menu::menu_scale,      0.5f, 1.0f, "%.2f"); }
                NAV_ROW { ImAdd::SliderFloat("Font Scale",    &settings::menu::font_scale,      0.5f, 2.0f, "%.2f"); }
                NAV_ROW { ImAdd::SliderFloat("Rounding",      &settings::menu::panel_rounding,  0.f,  20.f, "%.0f"); }
                NAV_ROW { ImAdd::SliderFloat("Sidebar Alpha", &settings::menu::sidebar_opacity, 0.1f, 1.f,  "%.2f"); }

                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                // ── TOPBAR ───────────────────────────────────────────────────
                NAV_ROW { MenuTheme::SectionHeader("TOPBAR"); }
                NAV_ROW { ImAdd::CheckBoxRow2("Show Version", &settings::menu::topbar_show_version, "Show Active", &settings::menu::topbar_show_active); }

                ImGui::PopStyleVar();
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    if (m_bClientWindowOpen && nav_page == 1)
    {
        {
            ImGuiWindow* mw = ImGui::FindWindowByName("##MatchaMain");
            const float gap  = 10.f;
            const float pw   = 340.f;
            const ImVec2 mpos = mw ? mw->Pos : ImVec2(io.DisplaySize.x * 0.5f - MenuTheme::GetWindowW() * 0.5f, io.DisplaySize.y * 0.5f - MenuTheme::GetWindowH() * 0.5f);
            const float menu_w = mw ? mw->Size.x : MenuTheme::GetWindowW();
            const float ph   = 400.f;
            ImVec2 prev_sz(pw, ph);
            if (ImGuiWindow* existing = ImGui::FindWindowByName("##QuannwarePreview"))
                prev_sz = existing->Size;
            ImGui::SetNextWindowPos(MenuTheme::TickSmoothDrag("##QuannwarePreview", prev_sz,
                ImVec2(mpos.x + menu_w + gap, mpos.y), MenuTheme::GetChildTitlebarH()), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(pw, ph), ImGuiCond_Once);
            ImGui::SetNextWindowSizeConstraints(ImVec2(240.f, 280.f), ImVec2(FLT_MAX, FLT_MAX));
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        bool client_window = ImGui::Begin("##QuannwarePreview", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove);
        ImGui::PopStyleVar(2);

        if (client_window)
        {
            ImRect window_bb(ImGui::GetCurrentWindow()->Rect());
            MenuTheme::DrawSatelliteChrome(ImGui::GetCurrentWindow(), window_bb, "ESP Preview");

            const float title_h = MenuTheme::GetChildTitlebarH();
            const ImVec2 canvas_origin = window_bb.Min + ImVec2(8.f, title_h + 4.f);
            ImVec2 preview_avail = window_bb.Max - canvas_origin - ImVec2(8.f, 8.f);
            if (preview_avail.x < 32.f) preview_avail.x = 32.f;
            if (preview_avail.y < 32.f) preview_avail.y = 32.f;

            ImGui::SetCursorScreenPos(canvas_origin);
            bool client_body = ImGui::BeginChild("client_body", preview_avail,
                ImGuiChildFlags_None,
                ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

            if (client_body)
            {
                static bool model_parsed = false;
                static c_obj_model parsed_model;
                static std::string last_loaded_user_id;

                static std::unordered_map<std::string, float> rotation_map;
                static std::unordered_map<std::string, float> pitch_map;
                static std::unordered_map<std::string, float> zoom_map;
                static std::unordered_map<std::string, float> ambient_map;
                static std::unordered_map<std::string, float> diffuse_map;

                // Gelato-style spin: drag accumulates velocity, velocity decays
                // each frame via dynamic_easing so release gives a momentum feel.
                static float s_spin_velocity = 0.f;  // rad/frame, decays to 0
                static float s_spin_offset   = 0.f;  // accumulated drag angle
                static bool  s_spin_dragging = false;

                const std::string local_user_id = "preview";
                c_avatar_3d_data* avatar_3d = c_avatar_3d_api::get().placeholder_data();
                e_avatar_3d_load_state load_state = e_avatar_3d_load_state::loaded;

                ImVec2 child_size = preview_avail;
                if (child_size.x < 32.f) child_size.x = 32.f;
                if (child_size.y < 32.f) child_size.y = 32.f;
                ImGui::Dummy(child_size);

                if (load_state == e_avatar_3d_load_state::failed) {
                    ImVec2 text_size = ImGui::CalcTextSize("Failed to load 3D avatar");
                    ImGui::SetCursorPos(ImVec2((child_size.x - text_size.x) * 0.5f, child_size.y * 0.5f));
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Failed to load 3D avatar");
                }
                else if (load_state == e_avatar_3d_load_state::loading || load_state == e_avatar_3d_load_state::not_loaded) {
                    const float now = static_cast<float>(ImGui::GetTime());
                    const char* dots[] = { "Loading avatar.", "Loading avatar..", "Loading avatar..." };
                    const char* lbl = dots[static_cast<int>(now * 2.f) % 3];
                    ImVec2 tsz = ImGui::CalcTextSize(lbl);
                    ImGui::SetCursorPos(ImVec2((child_size.x - tsz.x) * 0.5f, (child_size.y - tsz.y) * 0.5f));
                    ImGui::TextDisabled("%s", lbl);
                }
                else if (avatar_3d && avatar_3d->ready) {
                    if (last_loaded_user_id != local_user_id || !model_parsed) {
                        c_texture_cache::get().clear_user(local_user_id);

                        model_parsed = c_avatar_3d_api::get().parse_obj_model(avatar_3d->obj_data, parsed_model);
                        c_avatar_3d_api::debug_log("parse_obj user=%s ok=%d obj_bytes=%zu faces=%zu",
                            local_user_id.c_str(), model_parsed ? 1 : 0,
                            avatar_3d->obj_data.size(), parsed_model.faces.size());

                        if (model_parsed && !avatar_3d->mtl_data.empty()) {
                            bool mtl_parsed = c_avatar_3d_api::get().parse_mtl_data(avatar_3d->mtl_data, parsed_model, avatar_3d->texture_hashes);

                            if (mtl_parsed && !avatar_3d->texture_data.empty()) {
                                std::unordered_set<int> requested_indices;

                                for (const auto& mat_pair : parsed_model.materials) {
                                    int tex_idx = mat_pair.second.texture_index;
                                    if (tex_idx >= 0 && tex_idx < static_cast<int>(avatar_3d->texture_data.size()) &&
                                        !avatar_3d->texture_data[tex_idx].empty() &&
                                        requested_indices.find(tex_idx) == requested_indices.end()) {

                                        c_texture_cache::get().request_texture(local_user_id, tex_idx, avatar_3d->texture_data[tex_idx], true);
                                        requested_indices.insert(tex_idx);
                                    }
                                }
                            }
                        }
                        last_loaded_user_id = local_user_id;
                    }

                    ImDrawList* preview_draw = ImGui::GetWindowDrawList();
                    ImVec2 canvas_min = canvas_origin;
                    float preview_width = child_size.x;
                    float available_height = child_size.y;
                    ImVec2 canvas_center = ImVec2(canvas_min.x + preview_width * 0.5f, canvas_min.y + available_height * 0.5f);

                    if (pitch_map.find(local_user_id) == pitch_map.end()) pitch_map[local_user_id] = 0.0f;
                    if (zoom_map.find(local_user_id) == zoom_map.end()) zoom_map[local_user_id] = 1.2f;
                    if (ambient_map.find(local_user_id) == ambient_map.end()) ambient_map[local_user_id] = 1.0f;
                    if (diffuse_map.find(local_user_id) == diffuse_map.end()) diffuse_map[local_user_id] = 1.0f;

                    float& pitch = pitch_map[local_user_id];
                    float& zoom = zoom_map[local_user_id];
                    float& ambient = ambient_map[local_user_id];
                    float& diffuse = diffuse_map[local_user_id];

                    // ------------------------------------------------------------------
                    // Gelato-style spin logic
                    // Base rotation: time-driven like gelato (canvas.yaw = GetTime()*0.55)
                    // so it never accumulates floating-point drift across sessions.
                    // Drag spin: left-click + drag inside the canvas adds velocity;
                    // on release the velocity decays via dynamic_easing (speed/fps),
                    // giving a momentum "flick" feel identical to gelato.
                    // ------------------------------------------------------------------
                    {
                        const ImVec2 mouse = ImGui::GetIO().MousePos;
                        const ImRect canvas_rect(canvas_min, canvas_min + ImVec2(preview_width, available_height));
                        const bool in_canvas = canvas_rect.Contains(mouse);

                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && in_canvas)
                            s_spin_dragging = true;
                        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                            s_spin_dragging = false;

                        if (s_spin_dragging && ImGui::IsMouseDown(ImGuiMouseButton_Left))
                        {
                            // Scale mouse pixels → radians (0.008 rad/px ≈ gelato feel)
                            const float drag_delta = ImGui::GetIO().MouseDelta.x * 0.008f;
                            s_spin_velocity = drag_delta;   // hard-set each drag frame so
                            s_spin_offset  += drag_delta;   // offset tracks cursor exactly
                        }
                        else
                        {
                            // Decay velocity with gelato dynamic_easing: lerp toward 0
                            const float fps   = ImGui::GetIO().Framerate;
                            const float alpha = 8.f / (fps > 1.f ? fps : 60.f);
                            s_spin_velocity = s_spin_velocity + (0.f - s_spin_velocity) * alpha;
                            s_spin_offset  += s_spin_velocity;
                        }
                    }

                    // Final yaw: time-base (never drifts) + drag offset
                    const float rotation = static_cast<float>(ImGui::GetTime()) * 0.55f + s_spin_offset;

                    float preview_min_x = FLT_MAX, preview_min_y = FLT_MAX;
                    float preview_max_x = -FLT_MAX, preview_max_y = -FLT_MAX;

                    if (model_parsed && !parsed_model.vertices.empty()) {
                        float aabb_width = avatar_3d->aabb.max[0] - avatar_3d->aabb.min[0];
                        float aabb_height = avatar_3d->aabb.max[1] - avatar_3d->aabb.min[1];
                        float aabb_depth = avatar_3d->aabb.max[2] - avatar_3d->aabb.min[2];
                        float max_dim = max(aabb_width, max(aabb_height, aabb_depth));

                        float base_scale = 200.0f / max_dim;
                        float auto_fit_zoom = zoom;
                        if (max_dim > 10.0f) {
                            auto_fit_zoom = zoom * (10.0f / max_dim);
                        }

                        float scale = base_scale * auto_fit_zoom;

                        float cos_rot = std::cos(rotation);
                        float sin_rot = std::sin(rotation);
                        float cos_pitch = std::cos(pitch);
                        float sin_pitch = std::sin(pitch);

                        float aabb_center_x = (avatar_3d->aabb.min[0] + avatar_3d->aabb.max[0]) * 0.5f;
                        float aabb_center_y = (avatar_3d->aabb.min[1] + avatar_3d->aabb.max[1]) * 0.5f;
                        float aabb_center_z = (avatar_3d->aabb.min[2] + avatar_3d->aabb.max[2]) * 0.5f;

                        // Re-center on the model's actual projected silhouette rather than
                        // its raw AABB midpoint -- poses/accessories can make the AABB
                        // asymmetric (e.g. a raised arm or a hat), which would otherwise
                        // leave the character looking off-center in the preview even
                        // though the AABB itself is "centered".
                        ImVec2 render_center = canvas_center;
                        {
                            float proj_min_x = FLT_MAX, proj_max_x = -FLT_MAX;
                            float proj_min_y = FLT_MAX, proj_max_y = -FLT_MAX;
                            for (const auto& v : parsed_model.vertices) {
                                float x = v.x - aabb_center_x;
                                float y = v.y - aabb_center_y;
                                float z = v.z - aabb_center_z;

                                float rotated_x = x * cos_rot - z * sin_rot;
                                float rotated_z = x * sin_rot + z * cos_rot;
                                float final_y   = y * cos_pitch - rotated_z * sin_pitch;

                                proj_min_x = min(proj_min_x, rotated_x * scale);
                                proj_max_x = max(proj_max_x, rotated_x * scale);
                                proj_min_y = min(proj_min_y, final_y * scale);
                                proj_max_y = max(proj_max_y, final_y * scale);
                            }
                            if (proj_min_x <= proj_max_x && proj_min_y <= proj_max_y) {
                                render_center.x -= (proj_min_x + proj_max_x) * 0.5f;
                                render_center.y += (proj_min_y + proj_max_y) * 0.5f;
                            }
                        }

                        std::vector<std::tuple<float, int>> depth_sorted_faces;
                        depth_sorted_faces.reserve(parsed_model.faces.size());

                        for (size_t i = 0; i < parsed_model.faces.size(); i++) {
                            const auto& face = parsed_model.faces[i];
                            float avg_z = 0.0f;

                            for (int j = 0; j < 3; j++) {
                                const auto& v = parsed_model.vertices[face.vertex_indices[j]];
                                float x = v.x - aabb_center_x;
                                float y = v.y - aabb_center_y;
                                float z = v.z - aabb_center_z;

                                float rotated_x = x * cos_rot - z * sin_rot;
                                float rotated_z = x * sin_rot + z * cos_rot;
                                float final_z = y * sin_pitch + rotated_z * cos_pitch;

                                avg_z += final_z;
                            }
                            avg_z /= 3.0f;
                            depth_sorted_faces.push_back(std::make_tuple(avg_z, static_cast<int>(i)));
                        }

                        std::sort(depth_sorted_faces.begin(), depth_sorted_faces.end(),
                            [](const std::tuple<float, int>& a, const std::tuple<float, int>& b) { return std::get<0>(a) < std::get<0>(b); });

                        float light_dir_x = 0.5f;
                        float light_dir_y = 0.8f;
                        float light_dir_z = -0.3f;
                        float light_len = std::sqrt(light_dir_x * light_dir_x + light_dir_y * light_dir_y + light_dir_z * light_dir_z);
                        light_dir_x /= light_len;
                        light_dir_y /= light_len;
                        light_dir_z /= light_len;

                        for (const auto& face_tuple : depth_sorted_faces) {
                            int face_idx = std::get<1>(face_tuple);
                            const auto& face = parsed_model.faces[face_idx];

                            ImVec2 screen_points[3];
                            float transformed_vertices[3][3];
                            float uv_coords[3][2];
                            bool has_uvs = false;

                            for (int j = 0; j < 3; j++) {
                                const auto& v = parsed_model.vertices[face.vertex_indices[j]];
                                float x = v.x - aabb_center_x;
                                float y = v.y - aabb_center_y;
                                float z = v.z - aabb_center_z;

                                float rotated_x = x * cos_rot - z * sin_rot;
                                float rotated_z = x * sin_rot + z * cos_rot;
                                float final_y = y * cos_pitch - rotated_z * sin_pitch;
                                float final_z = y * sin_pitch + rotated_z * cos_pitch;

                                transformed_vertices[j][0] = rotated_x;
                                transformed_vertices[j][1] = final_y;
                                transformed_vertices[j][2] = final_z;

                                screen_points[j] = ImVec2(render_center.x + rotated_x * scale, render_center.y - final_y * scale);

                                preview_min_x = min(preview_min_x, screen_points[j].x);
                                preview_min_y = min(preview_min_y, screen_points[j].y);
                                preview_max_x = max(preview_max_x, screen_points[j].x);
                                preview_max_y = max(preview_max_y, screen_points[j].y);

                                if (face.texcoord_indices.size() >= 3) {
                                    int uv_idx = face.texcoord_indices[j];
                                    if (uv_idx >= 0 && uv_idx < static_cast<int>(parsed_model.tex_coords.size())) {
                                        uv_coords[j][0] = parsed_model.tex_coords[uv_idx].u;
                                        uv_coords[j][1] = parsed_model.tex_coords[uv_idx].v;
                                        has_uvs = true;
                                    }
                                    else {
                                        has_uvs = false;
                                        break;
                                    }
                                }
                            }

                            float v1x = transformed_vertices[1][0] - transformed_vertices[0][0];
                            float v1y = transformed_vertices[1][1] - transformed_vertices[0][1];
                            float v1z = transformed_vertices[1][2] - transformed_vertices[0][2];

                            float v2x = transformed_vertices[2][0] - transformed_vertices[0][0];
                            float v2y = transformed_vertices[2][1] - transformed_vertices[0][1];
                            float v2z = transformed_vertices[2][2] - transformed_vertices[0][2];

                            float face_normal_x = v1y * v2z - v1z * v2y;
                            float face_normal_y = v1z * v2x - v1x * v2z;
                            float face_normal_z = v1x * v2y - v1y * v2x;

                            float nlen = std::sqrt(face_normal_x * face_normal_x + face_normal_y * face_normal_y + face_normal_z * face_normal_z);
                            if (nlen > 0.0001f) {
                                face_normal_x /= nlen;
                                face_normal_y /= nlen;
                                face_normal_z /= nlen;
                            }

                            float dotProduct = face_normal_x * light_dir_x + face_normal_y * light_dir_y + face_normal_z * light_dir_z;
                            dotProduct = max(0.0f, dotProduct);
                            float lighting = ambient + (diffuse * dotProduct);
                            lighting = min(1.0f, max(0.4f, lighting));

                            float r = 0.5f, g = 0.5f, b = 0.5f;
                            c_decoded_texture* texture = nullptr;

                            if (!face.material_name.empty()) {
                                auto mat_it = parsed_model.materials.find(face.material_name);
                                if (mat_it != parsed_model.materials.end()) {
                                    const c_obj_material& material = mat_it->second;
                                    r = material.diffuse[0];
                                    g = material.diffuse[1];
                                    b = material.diffuse[2];

                                    if (material.texture_index >= 0 && has_uvs) {
                                        texture = c_texture_cache::get().get_texture(local_user_id, material.texture_index);
                                        if (!texture || !texture->ready.load(std::memory_order_acquire)) {
                                            texture = nullptr;
                                        }
                                    }
                                }
                            }

                            if (texture && has_uvs) {
                                ImVec2 min_pt = screen_points[0];
                                ImVec2 max_pt = screen_points[0];
                                for (int j = 1; j < 3; j++) {
                                    min_pt.x = min(min_pt.x, screen_points[j].x);
                                    min_pt.y = min(min_pt.y, screen_points[j].y);
                                    max_pt.x = max(max_pt.x, screen_points[j].x);
                                    max_pt.y = max(max_pt.y, screen_points[j].y);
                                }
                                int min_x = max(0, static_cast<int>(std::floor(min_pt.x)));
                                int min_y = max(0, static_cast<int>(std::floor(min_pt.y)));
                                int max_x = min(static_cast<int>(canvas_min.x + preview_width),  static_cast<int>(std::ceil(max_pt.x)));
                                int max_y = min(static_cast<int>(canvas_min.y + available_height), static_cast<int>(std::ceil(max_pt.y)));
                                float denom = (screen_points[1].y - screen_points[2].y) * (screen_points[0].x - screen_points[2].x) +
                                    (screen_points[2].x - screen_points[1].x) * (screen_points[0].y - screen_points[2].y);
                                if (std::abs(denom) > 0.0001f) {
                                    // Draw 2x2 pixel tiles -- same UV accuracy, half the draw calls
                                    constexpr int kTile = 1;
                                    for (int py = min_y; py <= max_y; py += kTile) {
                                        for (int px = min_x; px <= max_x; px += kTile) {
                                            // Sample at tile center for accuracy
                                            float sx = px + kTile * 0.5f;
                                            float sy = py + kTile * 0.5f;
                                            float w0 = ((screen_points[1].y - screen_points[2].y) * (sx - screen_points[2].x) +
                                                (screen_points[2].x - screen_points[1].x) * (sy - screen_points[2].y)) / denom;
                                            float w1 = ((screen_points[2].y - screen_points[0].y) * (sx - screen_points[2].x) +
                                                (screen_points[0].x - screen_points[2].x) * (sy - screen_points[2].y)) / denom;
                                            float w2 = 1.0f - w0 - w1;
                                            if (w0 >= -0.01f && w1 >= -0.01f && w2 >= -0.01f) {
                                                float u = w0 * uv_coords[0][0] + w1 * uv_coords[1][0] + w2 * uv_coords[2][0];
                                                float v = w0 * uv_coords[0][1] + w1 * uv_coords[1][1] + w2 * uv_coords[2][1];
                                                float texR, texG, texB, texA;
                                                texture->sample(u, v, texR, texG, texB, texA);
                                                if (texA > 0.01f) {
                                                    ImU32 pixel_color = IM_COL32(
                                                        static_cast<int>(min(1.f, texR * lighting) * 255),
                                                        static_cast<int>(min(1.f, texG * lighting) * 255),
                                                        static_cast<int>(min(1.f, texB * lighting) * 255),
                                                        static_cast<int>(texA * 255));
                                                    preview_draw->AddRectFilled(
                                                        ImVec2(static_cast<float>(px), static_cast<float>(py)),
                                                        ImVec2(static_cast<float>(px + kTile), static_cast<float>(py + kTile)),
                                                        pixel_color);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            else {
                                r = min(1.0f, r * lighting);
                                g = min(1.0f, g * lighting);
                                b = min(1.0f, b * lighting);

                                ImU32 fillColor = IM_COL32(static_cast<int>(r * 255),
                                    static_cast<int>(g * 255),
                                    static_cast<int>(b * 255), 255);

                                preview_draw->AddTriangleFilled(screen_points[0], screen_points[1], screen_points[2], fillColor);
                            }
                        }
                    }

                    if (preview_min_x < preview_max_x && preview_min_y < preview_max_y)
                    {
                        float left = preview_min_x;
                        float top = preview_min_y;
                        float right = preview_max_x;
                        float bottom = preview_max_y;

                        ImVec2 c1(left, top);
                        ImVec2 c2(right - left, bottom - top);

                        ImDrawList* draw = preview_draw;

                        if (settings::visuals::box_fill)
                        {
                            ImRect fill_rect(c1.x, c1.y, c1.x + c2.x, c1.y + c2.y);

                            if (settings::visuals::box_fill_gradient)
                            {
                                const float time = settings::visuals::box_fill_gradient_rotate
                                    ? static_cast<float>(ImGui::GetTime()) * settings::visuals::box_fill_speed : 0.f;
                                const float s = sinf(time);
                                const float c = cosf(time);

                                auto lerp_col = [&](ImU32 a, ImU32 b, float t) -> ImU32 {
                                    return ImGui::ColorConvertFloat4ToU32(ImLerp(
                                        ImGui::ColorConvertU32ToFloat4(a),
                                        ImGui::ColorConvertU32ToFloat4(b), t));
                                };

                                ImU32 col1 = ImGui::ColorConvertFloat4ToU32({ settings::visuals::box_fill_color_top[0],    settings::visuals::box_fill_color_top[1],    settings::visuals::box_fill_color_top[2],    settings::visuals::box_fill_color_top[3] });
                                ImU32 col2 = ImGui::ColorConvertFloat4ToU32({ settings::visuals::box_fill_color_bottom[0], settings::visuals::box_fill_color_bottom[1], settings::visuals::box_fill_color_bottom[2], settings::visuals::box_fill_color_bottom[3] });

                                ImU32 c_tl, c_tr, c_br, c_bl;
                                if (settings::visuals::box_fill_type == 0) {
                                    c_tl = c_bl = lerp_col(col1, col2, (s + 1.f) * 0.5f);
                                    c_tr = c_br = lerp_col(col1, col2, (c + 1.f) * 0.5f);
                                } else if (settings::visuals::box_fill_type == 1) {
                                    c_tl = c_tr = lerp_col(col1, col2, (s + 1.f) * 0.5f);
                                    c_bl = c_br = lerp_col(col1, col2, (c + 1.f) * 0.5f);
                                } else {
                                    c_tl = lerp_col(col1, col2, (s  + 1.f) * 0.5f);
                                    c_tr = lerp_col(col1, col2, (c  + 1.f) * 0.5f);
                                    c_br = lerp_col(col1, col2, (-s + 1.f) * 0.5f);
                                    c_bl = lerp_col(col1, col2, (-c + 1.f) * 0.5f);
                                }
                                draw->AddRectFilledMultiColor(
                                    ImVec2(fill_rect.Min.x + 1.f, fill_rect.Min.y + 1.f),
                                    ImVec2(fill_rect.Max.x - 1.f, fill_rect.Max.y - 1.f),
                                    c_tl, c_tr, c_br, c_bl);
                            }
                            else
                            {
                                ImU32 fill_col = ImGui::ColorConvertFloat4ToU32({
                                    settings::visuals::box_fill_color[0],
                                    settings::visuals::box_fill_color[1],
                                    settings::visuals::box_fill_color[2],
                                    settings::visuals::box_fill_color[3]
                                });
                                draw->AddRectFilled(fill_rect.Min, fill_rect.Max, fill_col);
                            }
                        }

                        if (settings::visuals::box)
                        {
                            ImU32 box_col = ImGui::ColorConvertFloat4ToU32(
                                {
                                    settings::visuals::box_color[0],
                                    settings::visuals::box_color[1],
                                    settings::visuals::box_color[2],
                                    settings::visuals::box_color[3]
                                }
                            );

                            if (settings::visuals::box_type == 0)
                            {
                                c1.x = std::round(c1.x);
                                c1.y = std::round(c1.y);
                                c2.x = std::round(c2.x);
                                c2.y = std::round(c2.y);

                                ImDrawListFlags old_flags = draw->Flags;
                                draw->Flags &= ~ImDrawListFlags_AntiAliasedFill;
                                draw->Flags |= ImDrawListFlags_AntiAliasedLines;

                                ImRect rect(c1.x, c1.y, c1.x + c2.x, c1.y + c2.y);

                                draw->AddRect(rect.Min, rect.Max, IM_COL32(0, 0, 0, box_col >> 24));
                                draw->AddRect({ rect.Min.x - 1.f, rect.Min.y - 1.f }, { rect.Max.x + 1.f, rect.Max.y + 1.f }, box_col);
                                draw->AddRect({ rect.Min.x - 2.f, rect.Min.y - 2.f }, { rect.Max.x + 2.f, rect.Max.y + 2.f }, IM_COL32(0, 0, 0, box_col >> 24));

                                draw->Flags = old_flags;
                            }
                            else
                            {
                                ImRect rect_bb(c1.x, c1.y, c1.x + c2.x, c1.y + c2.y);
                                helper::corner_box(draw, rect_bb.Min, rect_bb.Max, box_col, 1.f);
                            }
                        }
                        float box_center_x = left + (right - left) * 0.5f;
                        float box_top = top;
                        float box_bottom = bottom;
                        float box_left = std::round(left);
                        float box_right = std::round(right);
                        float box_width = box_right - box_left;
                        float box_height = box_bottom - box_top;

                        float actual_box_left = box_left;
                        float actual_box_right = box_right;
                        if (settings::visuals::box && settings::visuals::box_type == 0)
                        {
                            actual_box_left = c1.x;
                            actual_box_right = c1.x + c2.x;
                        }

                        ImFont* font = esp_font ? esp_font : ImGui::GetFont();
                        float font_size = esp_font ? font_config::tahoma::font_size : ImGui::GetFontSize();

                        if (settings::visuals::healthbar)
                        {
                            static float health_animation_time = 0.0f;
                            health_animation_time += ImGui::GetIO().DeltaTime * 0.5f;
                            if (health_animation_time > 1.0f) health_animation_time -= 1.0f;

                            float t = (std::sin(health_animation_time * 3.14159f * 2.0f) + 1.0f) * 0.5f;
                            float min_health = 0.2f;
                            float max_health = 1.0f;
                            float health_percent = min_health + (max_health - min_health) * t;

                            if (health_percent < 0.f) health_percent = 0.f;
                            if (health_percent > 1.f) health_percent = 1.f;

                            float health_percent_actual = health_percent * 100.f;
                            bool use_health_based = settings::visuals::health_based_healthbar;
                            bool use_gradient = settings::visuals::gradient_healthbar;

                            float color_r, color_g, color_b;
                            if (use_health_based)
                            {
                                if (health_percent_actual > 60.f)
                                {
                                    color_r = 0.f;
                                    color_g = 1.f;
                                    color_b = 0.f;
                                }
                                else if (health_percent_actual >= 50.f)
                                {
                                    color_r = 1.f;
                                    color_g = 0.647f;
                                    color_b = 0.f;
                                }
                                else if (health_percent_actual <= 20.f)
                                {
                                    color_r = 1.f;
                                    color_g = 0.f;
                                    color_b = 0.f;
                                }
                                else
                                {
                                    color_r = 1.f;
                                    color_g = 0.647f;
                                    color_b = 0.f;
                                }
                            }
                            else
                            {
                                color_r = settings::visuals::healthbar_color[0];
                                color_g = settings::visuals::healthbar_color[1];
                                color_b = settings::visuals::healthbar_color[2];
                            }

                            float transparency = 255.f;
                            float health_bar_height = box_height + 2.f;
                            draw->AddRectFilled(ImVec2(box_left - 7.f, box_top - 2.f), ImVec2(box_left - 3.f, box_bottom + 2.f), IM_COL32(0, 0, 0, 255));
                            if (health_percent > 0.f)
                            {
                                float health_fill_height = health_bar_height * health_percent;
                                ImVec2 fill_min = ImVec2(box_left - 6.f, box_bottom + 1.f - health_fill_height);
                                ImVec2 fill_max = ImVec2(box_left - 4.f, box_bottom + 1.f);

                                draw->AddRectFilled(fill_min, fill_max, IM_COL32(static_cast<int>(color_r * 255.f), static_cast<int>(color_g * 255.f), static_cast<int>(color_b * 255.f), static_cast<int>(transparency)));
                            }
                        }

                        if (settings::visuals::armorbar)
                        {
                            static float armor_animation_time = 0.0f;
                            armor_animation_time += ImGui::GetIO().DeltaTime * 0.4f;
                            if (armor_animation_time > 1.0f) armor_animation_time -= 1.0f;

                            float t = (std::sin(armor_animation_time * 3.14159f * 2.0f) + 1.0f) * 0.5f;
                            float min_armor = 0.0f;
                            float max_armor = 1.0f;
                            float armor_percent = min_armor + (max_armor - min_armor) * t;

                            if (armor_percent < 0.f) armor_percent = 0.f;
                            if (armor_percent > 1.f) armor_percent = 1.f;

                            float bar_height = 2.0f;
                            float bar_y = std::round(box_bottom) + 5.0f;
                            float bar_left = actual_box_left - 1.0f;
                            float bar_right = actual_box_right + 1.0f;
                            float bar_width = bar_right - bar_left;
                            float fill_width = bar_width * armor_percent;

                            ImDrawListFlags old_flags = draw->Flags;
                            draw->Flags &= ~ImDrawListFlags_AntiAliasedFill;
                            draw->Flags |= ImDrawListFlags_AntiAliasedLines;

                            draw->AddRectFilled(ImVec2(std::round(bar_left - 1.0f), std::round(bar_y - 1.0f)), ImVec2(std::round(bar_right + 1.0f), std::round(bar_y + bar_height + 1.0f)), IM_COL32(0, 0, 0, 255));
                            draw->AddRectFilled(ImVec2(std::round(bar_left), std::round(bar_y)), ImVec2(std::round(bar_right), std::round(bar_y + bar_height)), IM_COL32(45, 45, 45, 220));

                            ImU32 armor_col = ImGui::ColorConvertFloat4ToU32({
                                settings::visuals::armorbar_color[0],
                                settings::visuals::armorbar_color[1],
                                settings::visuals::armorbar_color[2],
                                settings::visuals::armorbar_color[3]
                                });

                            if (fill_width > 0.0f)
                            {
                                draw->AddRectFilled(ImVec2(std::round(bar_left), std::round(bar_y)), ImVec2(std::round(bar_left + fill_width), std::round(bar_y + bar_height)), armor_col);
                            }

                            draw->Flags = old_flags;
                        }

                        if (settings::visuals::name)
                        {
                            std::string name_to_display = "DisplayName";
                            float text_width = font->CalcTextSizeA(font_size, FLT_MAX, 0.f, name_to_display.c_str()).x;

                            ImU32 name_col = ImGui::ColorConvertFloat4ToU32(
                                {
                                    settings::visuals::name_color[0],
                                    settings::visuals::name_color[1],
                                    settings::visuals::name_color[2],
                                    settings::visuals::name_color[3]
                                }
                            );

                            helper::draw_text_outlined(draw, font, font_size,
                                ImVec2(box_center_x - text_width * 0.5f, box_top - font_size - 5.f),
                                name_col, name_to_display.c_str());
                        }

                        if (settings::visuals::distance)
                        {
                            static float distance_animation_time = 0.0f;
                            distance_animation_time += ImGui::GetIO().DeltaTime * 0.5f;
                            if (distance_animation_time > 1.0f) distance_animation_time -= 1.0f;

                            float t = (std::sin(distance_animation_time * 3.14159f * 2.0f) + 1.0f) * 0.5f;
                            float min_distance = 50.0f;
                            float max_distance = 100.0f;
                            float animated_distance = min_distance + (max_distance - min_distance) * t;

                            char distance_str[32];
                            if (settings::visuals::distance_measurement == 1)
                            {
                                float distance_meters = animated_distance * 0.28f;
                                std::snprintf(distance_str, sizeof(distance_str), "%.1fm", distance_meters);
                            }
                            else
                            {
                                std::snprintf(distance_str, sizeof(distance_str), "%.1f", animated_distance);
                            }

                            float text_width = font->CalcTextSizeA(font_size, FLT_MAX, 0.f, distance_str).x;

                            ImU32 distance_col = ImGui::ColorConvertFloat4ToU32(
                                {
                                    settings::visuals::distance_color[0],
                                    settings::visuals::distance_color[1],
                                    settings::visuals::distance_color[2],
                                    settings::visuals::distance_color[3]
                                }
                            );

                            float distance_y = box_bottom + 5.f;
                            if (settings::visuals::armorbar)
                            {
                                distance_y = std::round(box_bottom) + 5.0f + 2.0f + 3.0f;
                            }

                            helper::draw_text_outlined(draw, font, font_size,
                                ImVec2(box_center_x - text_width * 0.5f, distance_y),
                                distance_col, distance_str);
                        }

                        if (settings::visuals::tool)
                        {
                            std::string tool_name = "Sword";
                            float text_width = font->CalcTextSizeA(font_size, FLT_MAX, 0.f, tool_name.c_str()).x;
                            float tool_y = box_bottom + 5.f;

                            if (settings::visuals::armorbar)
                            {
                                tool_y = std::round(box_bottom) + 5.0f + 2.0f + 3.0f;
                            }

                            if (settings::visuals::distance)
                            {
                                tool_y += font_size + 2.f;
                            }

                            ImU32 tool_col = ImGui::ColorConvertFloat4ToU32(
                                {
                                    settings::visuals::tool_color[0],
                                    settings::visuals::tool_color[1],
                                    settings::visuals::tool_color[2],
                                    settings::visuals::tool_color[3]
                                }
                            );

                            helper::draw_text_outlined(draw, font, font_size,
                                ImVec2(box_center_x - text_width * 0.5f, tool_y),
                                tool_col, tool_name.c_str());
                        }

                        if (settings::visuals::flags)
                        {
                            float current_y = box_top;

                            static float state_animation_time = 0.0f;
                            state_animation_time += ImGui::GetIO().DeltaTime * 0.3f;
                            if (state_animation_time > 2.0f) state_animation_time -= 2.0f;

                            const char* state = "Running";
                            if (state_animation_time < 1.0f)
                            {
                                state = "Running";
                            }
                            else
                            {
                                state = "Idle";
                            }

                            char state_buffer[64];
                            std::snprintf(state_buffer, sizeof(state_buffer), "[%s]", state);

                            ImU32 state_col = ImGui::ColorConvertFloat4ToU32(
                                {
                                    settings::visuals::flags_state_colour[0],
                                    settings::visuals::flags_state_colour[1],
                                    settings::visuals::flags_state_colour[2],
                                    settings::visuals::flags_state_colour[3]
                                }
                            );

                            float text_width = font->CalcTextSizeA(font_size, FLT_MAX, 0.f, state_buffer).x;
                            float flag_x = box_right + 5.f;

                            helper::draw_text_outlined(draw, font, font_size, ImVec2(flag_x, current_y), state_col, state_buffer);
                        }

                        // ── Skeleton preview ──────────────────────────────────
                        if (settings::visuals::skeleton)
                        {
                            const ImU32 sk_col = ImGui::ColorConvertFloat4ToU32({
                                settings::visuals::skeleton_color[0],
                                settings::visuals::skeleton_color[1],
                                settings::visuals::skeleton_color[2],
                                settings::visuals::skeleton_color[3]
                            });
                            const ImU32 sk_out = IM_COL32(0, 0, 0, (sk_col >> 24) & 0xFF);

                            // Synthesise joint positions proportional to the bounding box
                            // Tuned to match a Roblox R15 character silhouette
                            const float cx    = box_center_x;
                            const float bw    = box_right - box_left;
                            const float h     = box_bottom - box_top;

                            const float head_top_y = box_top  + h * 0.02f;
                            const float head_bot_y = box_top  + h * 0.20f;
                            const float shldr_y    = box_top  + h * 0.26f;
                            const float elbow_y    = box_top  + h * 0.44f;
                            const float hand_y     = box_top  + h * 0.60f;
                            const float hip_y      = box_top  + h * 0.54f;
                            const float knee_y     = box_top  + h * 0.74f;
                            const float foot_y     = box_top  + h * 0.97f;

                            // Arms stay within the box width
                            const float arm_x  = bw * 0.22f;
                            // Legs stay close to centre
                            const float leg_x  = bw * 0.12f;

                            auto sk_line = [&](ImVec2 a, ImVec2 b) {
                                draw->AddLine(a, b, sk_out, 3.f);
                                draw->AddLine(a, b, sk_col, 1.f);
                            };

                            // Head → neck → hip (spine)
                            sk_line({cx, head_top_y}, {cx, head_bot_y});
                            sk_line({cx, head_bot_y}, {cx, hip_y});
                            // Left arm
                            sk_line({cx, shldr_y},         {cx - arm_x, shldr_y});
                            sk_line({cx - arm_x, shldr_y}, {cx - arm_x, elbow_y});
                            sk_line({cx - arm_x, elbow_y}, {cx - arm_x * 0.9f, hand_y});
                            // Right arm
                            sk_line({cx, shldr_y},         {cx + arm_x, shldr_y});
                            sk_line({cx + arm_x, shldr_y}, {cx + arm_x, elbow_y});
                            sk_line({cx + arm_x, elbow_y}, {cx + arm_x * 0.9f, hand_y});
                            // Left leg
                            sk_line({cx, hip_y},          {cx - leg_x, knee_y});
                            sk_line({cx - leg_x, knee_y}, {cx - leg_x * 0.9f, foot_y});
                            // Right leg
                            sk_line({cx, hip_y},          {cx + leg_x, knee_y});
                            sk_line({cx + leg_x, knee_y}, {cx + leg_x * 0.9f, foot_y});
                        }
                    }
                }
            }

            ImGui::EndChild(); // client_body
        }
        ImGui::End();
    }

    // (Explorer satellite panel moved to before BeginMainWindow -- see above)
    if (false && MenuTheme::show_explorer)
    {
        ImGuiWindow* mw_ex = ImGui::FindWindowByName("##MatchaMain");
        const float gap_ex  = 6.f;
        const float pw_side = 400.f;   // same width as Players / ESP Preview panels
        const ImVec2 mpos_ex = mw_ex ? mw_ex->Pos
            : ImVec2(io.DisplaySize.x * 0.5f - MenuTheme::GetWindowW() * 0.5f,
                     io.DisplaySize.y * 0.5f - MenuTheme::GetWindowH() * 0.5f);

        // Extends to the midpoint of Players (left) and ESP Preview (right).
        const float ex_x = mpos_ex.x - pw_side * 0.5f;
        const float ex_y = mpos_ex.y + MenuTheme::GetWindowH() + gap_ex;
        const float ex_w = pw_side * 0.5f + MenuTheme::GetWindowW() + pw_side * 0.5f;
        const float ex_h = 310.f;

        ImGui::SetNextWindowPos(ImVec2(ex_x, ex_y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(ex_w, ex_h), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        bool ex_win = ImGui::Begin("##ExplorerBottomPanel", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove);
        ImGui::PopStyleVar(2);

        if (ex_win)
        {
            ImRect ex_bb(ImGui::GetCurrentWindow()->Rect());
            MenuTheme::DrawSatelliteChrome(ImGui::GetCurrentWindow(), ex_bb, "Explorer");

            // Content area starts below the titlebar
            const float content_pad = 6.f;
            ImGui::SetCursorScreenPos(ImVec2(ex_bb.Min.x + content_pad,
                ex_bb.Min.y + MenuTheme::GetChildTitlebarH() + content_pad));

            const float avail_w = ex_w - content_pad * 2.f;
            const float avail_h = ex_h - MenuTheme::GetChildTitlebarH() - content_pad * 2.f;
            const float tree_w  = ImTrunc(avail_w * 0.55f);
            const float props_w = avail_w - tree_w - ImGui::GetStyle().ItemSpacing.x;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(content_pad, 3.f));

            // Toolbar (Refresh, Search)
            const float cursor_before_toolbar = ImGui::GetCursorPosY();
            explorer::explorer->render_settings();
            ImGui::Spacing();
            const float toolbar_h_used = ImGui::GetCursorPosY() - cursor_before_toolbar;
            const float tree_h = avail_h - toolbar_h_used;

            // Tree pane
            ImGui::BeginGroup();
            if (ImGui::BeginChild("##ex_bot_tree", ImVec2(tree_w, tree_h), true,
                ImGuiWindowFlags_HorizontalScrollbar))
            {
                if (explorer::explorer->root)
                    explorer::explorer->render_node(explorer::explorer->root);
                else
                    ImGui::TextDisabled("Click \"Refresh Explorer\" (top) to load the tree.");
            }
            ImGui::EndChild();
            ImGui::EndGroup();

            ImGui::SameLine();

            // Properties pane
            ImGui::BeginGroup();
            if (ImGui::BeginChild("##ex_bot_props", ImVec2(props_w, tree_h), true,
                ImGuiWindowFlags_HorizontalScrollbar))
            {
                explorer::explorer->render_properties();
                explorer::render_humanoid_scanner_panel(explorer::explorer.get());
                explorer::render_techy_skin_scanner_panel(explorer::explorer.get());
            }
            ImGui::EndChild();
            ImGui::EndGroup();

            ImGui::PopStyleVar();
        }
        ImGui::End();
    }

    OpenCloseAnim::ApplyZoom();
    ImGui::PopStyleVar();
}

void Menu::Shutdown()
{
    if (!m_bInitialized) return;

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void Menu::InvalidateDeviceObjects()
{
    if (!m_bInitialized) return;

    ImGui_ImplDX11_InvalidateDeviceObjects();
}

void Menu::CreateDeviceObjects()
{
    if (!m_bInitialized) return;

    ImGui_ImplDX11_CreateDeviceObjects();
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
bool Menu::HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (!m_bInitialized) return false;

#if 0
    if (uMsg == WM_KEYDOWN && wParam == VK_DELETE)
    {

        return true;
    }
#endif

    return ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
}

void Menu::DrawKeybinds()
{
    if (!settings::ui::keybinds)
        return;

    // Collect active keybind labels
    struct kb_entry_t { std::string label; };
    std::vector<kb_entry_t> entries;

    // enabled ptr is checked so entry disappears the moment the feature is toggled off
    auto push = [&](const char* label, int key, int act_mode, bool enabled) {
        if (!enabled) return;
        if (key == 0) return;
        if (act_mode == 2) return; // "always on" — no need to show
        std::string text = label;
        text += "  [";
        text += keybind::get_key_name(key);
        text += "]";
        entries.push_back({ std::move(text) });
    };

    push("Aimbot",       settings::aimbot::keybind,                  settings::aimbot::activation_mode,       settings::aimbot::enabled);
    push("Silent Aim",   settings::silentaim::keybind,               settings::silentaim::activation_mode,    settings::silentaim::enabled);
    push("Speed",        settings::movement::speedhack::keybind,     settings::movement::speedhack::activation_mode,  settings::movement::speedhack::enabled);
    push("Fly",          settings::movement::flyhack::keybind,       settings::movement::flyhack::activation_mode,    settings::movement::flyhack::enabled);
    push("Jump",         settings::movement::jumphack::keybind,      settings::movement::jumphack::activation_mode,   settings::movement::jumphack::enabled);
    push("Desync",       settings::movement::desync::keybind,        settings::movement::desync::activation_mode,     settings::movement::desync::enabled);
    push("Spin360",      settings::rage::spin360::keybind,           settings::rage::spin360::activation_mode,        settings::rage::spin360::enabled);
    push("Fake Lag",     settings::rage::fake_lag::keybind,          settings::rage::fake_lag::activation_mode,       settings::rage::fake_lag::enabled);
    push("Ghost Mode",   settings::rage::ghost_mode::keybind,        settings::rage::ghost_mode::activation_mode,     settings::rage::ghost_mode::enabled);
    push("Super Punch",  settings::rage::super_punch::keybind,       settings::rage::super_punch::activation_mode,    settings::rage::super_punch::enabled);
    push("Low Grav",     settings::rage::low_gravity_jump::keybind,  settings::rage::low_gravity_jump::activation_mode, settings::rage::low_gravity_jump::enabled);
    push("Fling",        settings::rage::fling::keybind,             settings::rage::fling::activation_mode,          settings::rage::fling::enabled);
    push("Rapid Click",  settings::rage::rapid_click::keybind,       settings::rage::rapid_click::activation_mode,    settings::rage::rapid_click::enabled);
    push("Freeze",       settings::exploits::freezeplayer::keybind,  settings::exploits::freezeplayer::activation_mode, settings::exploits::freezeplayer::enabled);
    push("Triggerbot",   settings::triggerbot::keybind,              settings::triggerbot::activation_mode,           settings::triggerbot::enabled);
    push("Colorbot",     settings::colorbot::keybind,                settings::colorbot::activation_mode,             settings::colorbot::enabled);
    push("Bhop",         settings::movement::bhop::keybind,          settings::movement::bhop::activation_mode,       settings::movement::bhop::enabled);

    if (entries.empty())
        return;

    ImDrawList* dl  = ImGui::GetBackgroundDrawList();
    ImFont*     fnt = ImGui::GetFont();
    const float fs  = ImGui::GetFontSize();
    const float pad = 6.f;
    const float line_h = fs + 2.f;

    // Anchor: bottom-right of screen
    const ImGuiIO& io = ImGui::GetIO();
    float x = io.DisplaySize.x - pad - 150.f;
    float y = io.DisplaySize.y - pad - static_cast<float>(entries.size()) * line_h;

    for (const auto& e : entries)
    {
        // Shadow
        dl->AddText(fnt, fs, ImVec2(x + 1.f, y + 1.f), IM_COL32(0, 0, 0, 180), e.label.c_str());
        // Text
        dl->AddText(fnt, fs, ImVec2(x, y), IM_COL32(255, 255, 255, 220), e.label.c_str());
        y += line_h;
    }
}

