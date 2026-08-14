#include "pch.h"
#include "AimTarget.h"
#include "AimFov.h"
#include "app/Settings.h"
#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/math/Math.h"
#include "core/roblox/classes/Classes.h"
#include "core/player/PlayerHandler.h"
#include "features/visuals/RaycastEngine.h"
#include "features/visuals/havoc/HavocWorldEsp.h"
#include "features/games/PhantomForces.h"
#include "renderer/Renderer.h"
#include "imgui.h"
#include <Windows.h>
#include <cmath>
#include <cfloat>
#include <cstdint>

namespace Cheat {
namespace Features {
namespace AimTarget {

	namespace {
		using Config = Settings::AimbotConfig;

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
	} // namespace

	Scene Resolve()
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
					if (Games::PhantomForces::IsActivePlace())
						s.local_team_folder = PlayerHandler::LocalTeamFolder();
					else
						s.local_team_folder = PlayerHandler::ResolveTeamFolder(s.local_char);
				}
			}
		}

		s.ok = true;
		return s;
	}

	bool HitchanceOk(const Config& cfg)
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

	void Clear()
	{
		s_target = 0;
		s_pending = 0;
	}

	std::uint64_t Current() { return s_target; }
	int CurrentPart() { return s_aim_part; }
	Vector3 CurrentPoint() { return s_aim_point; }

	bool Select(const Config& cfg, const Scene& sc,
		Vector3& out_world, Vector2& out_screen)
	{
		ImVec2 anchor = AimFov::Anchor(cfg);
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

} // namespace AimTarget
} // namespace Features
} // namespace Cheat
