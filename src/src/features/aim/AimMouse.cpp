#include "pch.h"
#define NOMINMAX
#include "AimMouse.h"

#include "features/GameCursor.h"

#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <random>
#include <thread>

namespace Cheat {
    namespace Features {

        namespace {

            using Config = Settings::AimbotConfig;

            // ----------------------------------------------------------------
            // gelato константы (aimbot.h defaults), которых нет в нашем конфиге
            // ----------------------------------------------------------------
            constexpr float k_speed_multiplier   = 1.0f;   // gelato default
            constexpr float k_humanize_strength  = 1.0f;   // gelato default
            constexpr float k_humanize_fatigue   = 0.3f;   // gelato default
            constexpr float k_gelato_tick_hz     = 240.0f; // gelato worker тикает на 240Hz

            // наш выбор кривой (GUI): 0 linear, 1 exponential (дефолт), 2 spring, 3 bezier
            constexpr int k_curve_linear      = 0;
            constexpr int k_curve_exponential = 1;
            constexpr int k_curve_spring      = 2;
            constexpr int k_curve_bezier      = 3;

            // индексы gelato aim_style_t: linear=0, spring=1, exponential=2, cubed=3,
            // bezier=4, humanized=5
            constexpr int k_style_exponential = 2;

            // ----------------------------------------------------------------
            // gelato sdk/math/math.h — alpha-функции
            // ----------------------------------------------------------------
            float alpha_linear(float t) { return t; }

            float alpha_exp(float t) { return 1.0f - std::exp(-t * 4.0f); }

            float alpha_cubed(float t)
            {
                float inv = 1.0f - t;
                return 1.0f - inv * inv * inv;
            }

            float bezier_component(float t, float a, float b, float c, float d)
            {
                float u = 1.0f - t;
                return u * u * u * a + 3.0f * u * u * t * b + 3.0f * u * t * t * c + t * t * t * d;
            }

            float alpha_bezier(float t)
            {
                float lo = 0.0f, hi = 1.0f, mid = t;
                for (int i = 0; i < 16; ++i)
                {
                    mid = (lo + hi) * 0.5f;
                    float x = bezier_component(mid, 0.0f, 0.42f, 0.58f, 1.0f);
                    if (x < t) lo = mid; else hi = mid;
                }
                return std::clamp(bezier_component(mid, 0.0f, 0.0f, 1.0f, 1.0f), 0.0f, 1.0f);
            }

            float alpha_humanized(float t)
            {
                float lo = 0.0f, hi = 1.0f, mid = t;
                for (int i = 0; i < 12; ++i)
                {
                    mid = (lo + hi) * 0.5f;
                    float x = bezier_component(mid, 0.0f, 0.25f, 0.25f, 1.0f);
                    if (x < t) lo = mid; else hi = mid;
                }
                return std::clamp(bezier_component(mid, 0.0f, 0.1f, 1.0f, 1.0f), 0.0f, 1.0f);
            }

            // gelato alpha_from_style: t зажат в 0.01..1, exponential — дефолт
            float alpha_from_style(int style, float t)
            {
                t = std::clamp(t, 0.01f, 1.0f);
                switch (style)
                {
                case 2:  return alpha_exp(t);        // exponential (gelato default)
                case 3:  return alpha_cubed(t);      // cubed
                case 4:  return alpha_bezier(t);     // bezier
                case 5:  return alpha_humanized(t);  // humanized
                case 1:  return alpha_exp(t);        // spring → alpha_exp в gelato
                case 0:
                default: return alpha_linear(t);     // linear
                }
            }

            // наша кривая (0..3) → стиль gelato
            int curve_to_style(int curve)
            {
                switch (curve)
                {
                case k_curve_linear:      return 0; // linear
                case k_curve_spring:      return 1; // spring
                case k_curve_bezier:      return 4; // bezier
                case k_curve_exponential:
                default:                  return 2; // exponential (дефолт)
                }
            }

            // gelato spring_update — 1:1
            void spring_update(float& pos, float& vel, float target,
                               float stiffness, float damping, float dt)
            {
                if (dt <= 0.0f)
                    dt = 0.016f;
                float force = -stiffness * (pos - target) - damping * vel;
                vel += force * dt;
                pos += vel * dt;
            }

            // состояние пружины на ось (px/s), живёт между тиками
            float s_spring_vx = 0.0f;
            float s_spring_vy = 0.0f;
            constexpr float k_spring_stiffness = 140.0f;
            constexpr float k_spring_damping   = 16.0f;

            // ----------------------------------------------------------------
            // gelato state (aimbot_feature_t поля, mouse-часть) — трогает ТОЛЬКО worker
            // ----------------------------------------------------------------
            bool s_active = false;
            std::chrono::steady_clock::time_point s_lock_started{};
            std::chrono::steady_clock::time_point s_last_tick{};

            // gelato _input_move_mouse: накопление дробных шагов между SendInput
            float s_accum_x = 0.0f;
            float s_accum_y = 0.0f;

            // ----------------------------------------------------------------
            // публикация цели из рендер-потока в worker
            // ----------------------------------------------------------------
            std::mutex s_state_mutex;
            Config s_published_cfg{};
            Vector2 s_published_screen{};
            bool s_has_target = false;
            std::atomic<bool> s_worker_started{ false };

            // gelato get_cursor: ОДИН источник курсора и для выбора цели, и для движения.
            // Слои в GameCursor::AimCursor: мышь игры из памяти → центр вьюпорта в
            // first person → OS-курсор (gelato _input_cursor).
            bool get_cursor(float& out_x, float& out_y)
            {
                return GameCursor::AimCursor(out_x, out_y);
            }

            // gelato _input_move_mouse — 1:1
            void move_mouse(float dx, float dy)
            {
                s_accum_x += dx;
                s_accum_y += dy;

                LONG ix = static_cast<LONG>(s_accum_x);
                LONG iy = static_cast<LONG>(s_accum_y);

                if (ix == 0 && iy == 0)
                    return;

                s_accum_x -= static_cast<float>(ix);
                s_accum_y -= static_cast<float>(iy);

                INPUT input{};
                input.type = INPUT_MOUSE;
                input.mi.dwFlags = MOUSEEVENTF_MOVE;
                input.mi.dx = ix;
                input.mi.dy = iy;
                SendInput(1, &input, sizeof(INPUT));
            }

        } // namespace

        namespace AimMouse {

            // один тик gelato mouse branch (on_worker_tick, else-ветка mouse) — 1:1.
            // Вызывается ТОЛЬКО из worker-потока на 240Hz.
            void tick_mouse(const Config& cfg, const Vector2& best_screen)
            {
                float cursor_x = 0.0f, cursor_y = 0.0f;
                if (!get_cursor(cursor_x, cursor_y))
                    return;

                auto now = std::chrono::steady_clock::now();

                // gelato: при первом локе запоминаем время начала и чистим стейт
                if (!s_active)
                {
                    s_active = true;
                    s_lock_started = now;
                    s_last_tick = {};
                    s_accum_x = 0.0f;
                    s_accum_y = 0.0f;
                    s_spring_vx = 0.0f;
                    s_spring_vy = 0.0f;
                }

                auto hold_ms =
                    std::chrono::duration<float, std::milli>(now - s_lock_started).count();

                // gelato: dt с прошлого тика, дефолт 1/240, зажат в (0, 0.1]
                float dt = s_last_tick.time_since_epoch().count()
                    ? std::chrono::duration<float>(now - s_last_tick).count()
                    : (1.0f / k_gelato_tick_hz);
                s_last_tick = now;
                if (dt <= 0.0f || dt > 0.1f)
                    dt = 1.0f / k_gelato_tick_hz;

                // gelato make_alpha — 1:1 (smoothing → alpha по стилю, humanize ramp/fatigue)
                auto make_alpha = [&](float smoothing) -> float {
                    float a = 1.0f;

                    if (cfg.smooth_enabled)
                    {
                        float half_life = smoothing * 0.05f;
                        float lambda = std::log(2.0f) / std::max(half_life, 0.001f);
                        float t = std::clamp(
                            1.0f - std::exp(-lambda * dt * k_speed_multiplier), 0.005f, 1.0f);
                        a = alpha_from_style(curve_to_style(cfg.aim_curve), t);
                    }
                    // smoothing выключен — сырой инстант-двиг (alpha = 1)

                    if (cfg.humanize)
                    {
                        float reaction = cfg.reaction_ms;
                        if (reaction > 0.0f)
                        {
                            float ramp = std::clamp(hold_ms / reaction, 0.0f, 1.0f);
                            a *= ramp * ramp;
                        }

                        float f = std::clamp(hold_ms / 4000.0f, 0.0f, 1.0f)
                            * std::clamp(k_humanize_fatigue, 0.0f, 1.0f);
                        a *= (1.0f - f * 0.65f);

                        a = std::clamp(a, 0.01f, 1.0f);
                    }

                    return a;
                };

                float alpha_x = make_alpha(cfg.smooth_x);
                float alpha_y = make_alpha(cfg.smooth_y);

                // gelato mouse branch: delta до цели, умноженный на alpha.
                // spring-кривая — исключение: gelato spring_update гоняем прямо на
                // остаточной ошибке (позицию синхроним с фактической, скорость
                // копим между тиками — получается честный пружинный довод).
                float err_x = best_screen.x - cursor_x;
                float err_y = best_screen.y - cursor_y;
                float dx, dy;

                if (cfg.smooth_enabled && cfg.aim_curve == k_curve_spring)
                {
                    float px = err_x, py = err_y;
                    spring_update(px, s_spring_vx, 0.0f,
                                  k_spring_stiffness, k_spring_damping, dt);
                    spring_update(py, s_spring_vy, 0.0f,
                                  k_spring_stiffness, k_spring_damping, dt);
                    dx = err_x - px;
                    dy = err_y - py;
                }
                else
                {
                    dx = err_x * alpha_x;
                    dy = err_y * alpha_y;
                }

                // gelato humanize jitter — 1:1
                if (cfg.humanize)
                {
                    static std::mt19937 rng{ std::random_device{}() };
                    float strength = std::clamp(k_humanize_strength, 0.0f, 3.0f);
                    std::uniform_real_distribution<float> scale(
                        1.0f - 0.18f * strength, 1.0f + 0.18f * strength);
                    std::uniform_real_distribution<float> noise(
                        -0.85f * strength, 0.85f * strength);
                    dx *= scale(rng);
                    dx += noise(rng);
                    dy *= scale(rng);
                    dy += noise(rng);
                }

                // sensitivity — множитель скорости довода
                const float sens = std::clamp(cfg.aim_sensitivity, 0.05f, 5.0f);
                dx *= sens;
                dy *= sens;

                // gelato: max_step = max(speed*8, 1) px на тик 240Hz
                float max_step = std::max(k_speed_multiplier * 8.0f, 1.0f);
                dx = std::clamp(dx, -max_step, max_step);
                dy = std::clamp(dy, -max_step, max_step);

                move_mouse(dx, dy);
            }

            // gelato scheduler: aimbot тикает на 240Hz в своём потоке (tick_hz() = 240).
            // Маленькие шаги каждые ~4мс — стабильная замкнутая петля, камера не фликает.
            void worker_loop()
            {
                const auto tick_dt = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                    std::chrono::duration<float>(1.0f / k_gelato_tick_hz));

                auto next = std::chrono::steady_clock::now();
                for (;;)
                {
                    std::this_thread::sleep_until(next);
                    next += tick_dt;

                    Config cfg;
                    Vector2 best_screen{};
                    {
                        std::lock_guard<std::mutex> lock(s_state_mutex);
                        if (!s_has_target)
                        {
                            // gelato clear_target: цель потеряна — сброс лок-стейта
                            if (s_active)
                            {
                                s_active = false;
                                s_lock_started = {};
                                s_last_tick = {};
                                s_accum_x = 0.0f;
                                s_accum_y = 0.0f;
                            }
                            continue;
                        }
                        cfg = s_published_cfg;
                        best_screen = s_published_screen;
                    }

                    tick_mouse(cfg, best_screen);
                }
            }

            void ensure_worker()
            {
                bool expected = false;
                if (s_worker_started.compare_exchange_strong(expected, true))
                {
                    std::thread(worker_loop).detach();
                }
            }

            // рендер-поток: публикуем цель, worker подхватит на ближайшем тике
            void Publish(const Config& cfg, const Vector2& best_screen)
            {
                ensure_worker();
                std::lock_guard<std::mutex> lock(s_state_mutex);
                s_published_cfg = cfg;
                s_published_screen = best_screen;
                s_has_target = true;
            }

            // цель потеряна / ключ отпущен — worker сбросит лок-стейт на следующем тике
            void Reset()
            {
                std::lock_guard<std::mutex> lock(s_state_mutex);
                s_has_target = false;
            }

            // единый источник курсора для аима (выбор цели + движение)
            bool CursorPos(float& out_x, float& out_y)
            {
                return get_cursor(out_x, out_y);
            }

        } // namespace AimMouse
    } // namespace Features
} // namespace Cheat
