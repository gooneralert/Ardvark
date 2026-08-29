// music_player host bridge — implements music_player_host.h against the
// jewsploit overlay (Renderer window + D3D device + ImGui context).
// Adapted from the player's preview host so DrawMusicPlayer() can render
// inside the existing ImGui frame.

#include "music_player_host.h"

#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "renderer/Renderer.h"
#include "gui/resources/fonts/fonts.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <unordered_map>

namespace {

ImFont* g_regular = nullptr;
ImFont* g_bold = nullptr;

struct SpringState {
    float value = 0.0f;
    float velocity = 0.0f;
};

std::unordered_map<ImGuiID, float> g_animations;
std::unordered_map<ImGuiID, SpringState> g_springs;

} // namespace

namespace music_host {

ImVec2 Measure(ImFont* font, float size, const char* text) {
    ImFont* selected = font ? font : ImGui::GetFont();
    return selected->CalcTextSizeA(size, FLT_MAX, 0.0f, text ? text : "");
}

void DrawText(ImDrawList* drawList, ImFont* font, float size,
              ImVec2 position, ImU32 color, const char* text) {
    drawList->AddText(font ? font : ImGui::GetFont(), size, position,
                      color, text ? text : "");
}

void DrawShadow(ImDrawList* drawList, ImVec2 min, ImVec2 max, float rounding,
                int layers, float spread, float strength) {
    for (int index = layers; index >= 1; --index) {
        const float amount = static_cast<float>(index) / static_cast<float>(layers);
        const float grow = spread * amount;
        const int alpha = static_cast<int>(strength * 255.0f *
            (1.0f - amount) * (1.0f - amount) / layers * 3.0f);
        if (alpha <= 0) continue;
        drawList->AddRectFilled(
            ImVec2(min.x - grow, min.y - grow + 5.0f),
            ImVec2(max.x + grow, max.y + grow + 5.0f),
            IM_COL32(0, 0, 0, alpha), rounding + grow);
    }
}

namespace animation {

float Anim(ImGuiID id, bool enabled, float speed) {
    float delta = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
    if (delta <= 0.0f) delta = 1.0f / 60.0f;
    const float target = enabled ? 1.0f : 0.0f;
    auto [entry, inserted] = g_animations.try_emplace(id, target);
    entry->second += (target - entry->second) * (1.0f - std::exp(-speed * delta));
    return entry->second;
}

float SpringF(ImGuiID id, float target, float speed, float damping) {
    float delta = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.05f);
    if (delta <= 0.0f) delta = 1.0f / 60.0f;
    speed = std::max(speed, 0.01f);
    damping = std::max(damping, 0.0f);

    auto [entry, inserted] = g_springs.try_emplace(id, SpringState{ target, 0.0f });
    SpringState& state = entry->second;
    if (inserted) return state.value;

    const float previous = state.value;
    const float omegaSquared = speed * speed;
    const float dampingTerm = 1.0f + 2.0f * delta * damping * speed;
    const float targetTerm = delta * delta * omegaSquared;
    const float inverse = 1.0f / (dampingTerm + targetTerm);
    state.value = (dampingTerm * previous + delta * state.velocity +
                   targetTerm * target) * inverse;
    state.velocity = (state.velocity + delta * omegaSquared *
                      (target - previous)) * inverse;
    if (std::abs(target - state.value) < 0.0005f &&
        std::abs(state.velocity) < 0.0005f) {
        state = { target, 0.0f };
    }
    return state.value;
}

void SetSpring(ImGuiID id, float value) {
    g_springs[id] = { value, 0.0f };
}

float ClickBounce(ImGuiID id, bool triggered) {
    static std::unordered_map<ImGuiID, float> elapsed;
    auto [entry, inserted] = elapsed.try_emplace(id, -1.0f);
    if (triggered) entry->second = 0.0f;
    if (entry->second < 0.0f) return 1.0f;

    float delta = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
    if (delta <= 0.0f) delta = 1.0f / 60.0f;
    entry->second += delta;
    constexpr float duration = 0.28f;
    if (entry->second >= duration) {
        entry->second = -1.0f;
        return 1.0f;
    }
    const float progress = entry->second / duration;
    const float amplitude = 0.16f * std::exp(-5.0f * progress);
    return 1.0f - amplitude * std::cos(9.0f * progress);
}

float ClickGlow(ImGuiID id, bool triggered) {
    static std::unordered_map<ImGuiID, float> elapsed;
    auto [entry, inserted] = elapsed.try_emplace(id, -1.0f);
    if (triggered) entry->second = 0.0f;
    if (entry->second < 0.0f) return -1.0f;
    float delta = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
    if (delta <= 0.0f) delta = 1.0f / 60.0f;
    entry->second += delta;
    constexpr float duration = 0.42f;
    if (entry->second >= duration) { entry->second = -1.0f; return -1.0f; }
    return entry->second / duration;
}

float PressPulse(ImGuiID id, bool triggered) {
    static std::unordered_map<ImGuiID, float> elapsed;
    auto [entry, inserted] = elapsed.try_emplace(id, -1.0f);
    if (triggered) entry->second = 0.0f;
    if (entry->second < 0.0f) return 0.0f;
    float delta = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
    if (delta <= 0.0f) delta = 1.0f / 60.0f;
    entry->second += delta;
    constexpr float duration = 0.22f;
    if (entry->second >= duration) { entry->second = -1.0f; return 0.0f; }
    const float p = entry->second / duration;
    return (1.0f - p) * (1.0f - p);   // quick attack, ease-out decay
}

} // namespace animation

namespace overlay {

namespace {

// Simulated OS-fullscreen state for the player card. The overlay window
// already spans the whole game client rect, so "fullscreen" just means the
// card is laid out to fill the overlay's work area instead of its normal
// floating size (player.cpp handles this via IsFullscreenWindow()).
bool g_playerWindowFull = false;

} // namespace

HWND GetOverlayWindow() { return Cheat::Renderer::GetHwnd(); }
void* GetD3DDevice() { return Cheat::Renderer::GetDevice(); }

// The overlay window's geometry is owned by Renderer (it follows the game
// client rect), so there is no separate OS window to make borderless. Instead
// this toggles the player's in-overlay fullscreen state: the restore button in
// the fullscreen header acts like a maximize/restore pair.
void ToggleFullscreenWindow() {
    g_playerWindowFull = !g_playerWindowFull;
}
bool IsFullscreenWindow() { return g_playerWindowFull; }

ImFont* GetFont(int index) {
    if (index == 0) return fonts::music_regular ? fonts::music_regular : ImGui::GetFont();
    if (index == 1) return fonts::music_bold ? fonts::music_bold : ImGui::GetFont();
    return nullptr;
}
ImFont* GetMusicRegularFont() {
    return fonts::music_regular ? fonts::music_regular : ImGui::GetFont();
}
ImFont* GetMusicBoldFont() {
    return fonts::music_bold ? fonts::music_bold
                             : (fonts::music_regular ? fonts::music_regular
                                                     : ImGui::GetFont());
}

} // namespace overlay
} // namespace music_host


