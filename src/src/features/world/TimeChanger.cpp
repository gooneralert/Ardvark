#include "pch.h"
#include "TimeChanger.h"
#include "LightingInvalidate.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"
#include "app/Settings.h"

namespace Cheat {
	namespace Features {

		namespace {
			void write_clock_time(std::uint64_t lighting, float t)
			{
				const std::uint64_t addr = lighting + ::Lighting::ClockTime;
				const float  cur_f = g_Memory.Read<float>(addr);
				const double cur_d = g_Memory.Read<double>(addr);
				const bool f_ok = cur_f >= 0.f && cur_f <= 24.f;
				const bool d_ok = cur_d >= 0.0 && cur_d <= 24.0;

				if (d_ok && !f_ok)
					LightingInvalidate::force_write<double>(addr, static_cast<double>(t));

				else
					LightingInvalidate::force_write<float>(addr, t);
			}
		}

		bool TimeChanger::Apply(std::uint64_t lighting)
		{
			static bool s_time_was_on = false;

			const auto& w = Cheat::g_Settings.world;

			if (w.time_changer)
			{
				float t = w.clock_time;
				if (t < 0.f) t = 0.f;
				if (t > 24.f) t = 24.f;

				write_clock_time(lighting, t);

				constexpr float PI = 3.14159265f;
				const float sun_angle = (t / 24.f - 0.25f) * 2.f * PI;
				const float sun_y = std::sin(sun_angle);
				const float sun_x = 0.f;
				const float sun_z = -std::cos(sun_angle);

				const Vector3 sun_pos(sun_x, sun_y, sun_z);
				const Vector3 moon_pos(-sun_x, -sun_y, -sun_z);
				const Vector3 light_dir = (sun_y > 0.f) ? sun_pos : moon_pos;

				LightingInvalidate::force_write<Vector3>(lighting + ::Lighting::SunPosition, sun_pos);
				LightingInvalidate::force_write<Vector3>(lighting + ::Lighting::MoonPosition, moon_pos);
				LightingInvalidate::force_write<Vector3>(lighting + ::Lighting::LightDirection, light_dir);

				const float h = sun_y;
				Vector3 grad_top, grad_bot;
				float brightness = 2.f;
				Color3 outdoor;
				Color3 ambient;
				Color3 light_col;

				if (h > 0.15f)
				{
					grad_top = Vector3(0.35f, 0.65f, 0.95f);
					grad_bot = Vector3(0.75f, 0.85f, 0.95f);
					brightness = 2.0f;
					outdoor = Color3(0.6f + h * 0.1f, 0.6f + h * 0.1f, 0.62f + h * 0.1f);
					ambient = Color3(0.45f, 0.45f, 0.50f);
					light_col = Color3(1.f, 0.96f, 0.90f);
				}

				else if (h > 0.0f)
				{
					const float st = h / 0.15f;
					grad_top = Vector3(0.15f + st * 0.2f, 0.15f + st * 0.5f, 0.25f + st * 0.7f);
					grad_bot = Vector3(0.95f, 0.45f + st * 0.4f, 0.25f + st * 0.7f);
					brightness = 1.5f;
					outdoor = Color3(0.6f + h * 0.1f, 0.6f + h * 0.1f, 0.62f + h * 0.1f);
					ambient = Color3(0.35f + st * 0.1f, 0.30f + st * 0.1f, 0.35f + st * 0.1f);
					light_col = Color3(1.f, 0.55f + st * 0.4f, 0.35f + st * 0.4f);
				}

				else
				{
					float nt = (-h) / 0.3f;
					if (nt > 1.0f) nt = 1.0f;
					const float inv = 1.0f - nt;
					grad_top = Vector3(0.05f + inv * 0.1f, 0.05f + inv * 0.1f, 0.12f + inv * 0.13f);
					grad_bot = Vector3(0.08f + inv * 0.07f, 0.08f + inv * 0.07f, 0.15f + inv * 0.1f);
					brightness = 1.5f;
					outdoor = Color3(0.15f + inv * 0.2f, 0.15f + inv * 0.2f, 0.22f + inv * 0.2f);
					ambient = Color3(0.08f + inv * 0.12f, 0.08f + inv * 0.12f, 0.14f + inv * 0.12f);
					light_col = Color3(0.55f, 0.60f, 0.85f);
				}

				LightingInvalidate::force_write<Vector3>(lighting + ::Lighting::GradientTop, grad_top);
				LightingInvalidate::force_write<Vector3>(lighting + ::Lighting::GradientBottom, grad_bot);
				LightingInvalidate::force_write<Color3>(lighting + ::Lighting::OutdoorAmbient, outdoor);
				LightingInvalidate::force_write<Color3>(lighting + ::Lighting::Ambient, ambient);
				LightingInvalidate::force_write<Color3>(lighting + ::Lighting::LightColor, light_col);
				LightingInvalidate::force_write<float>(lighting + ::Lighting::Brightness, brightness);

				s_time_was_on = true;
				return true;
			}

			else if (s_time_was_on)
			{
				s_time_was_on = false;
				return true;
			}

			return false;
		}

	}
}
