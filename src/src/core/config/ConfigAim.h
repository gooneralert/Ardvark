#pragma once

#include "ConfigIo.h"
#include "app/Settings.h"

namespace Cheat {
namespace Config {
namespace detail {

// aimbot блок, префикс типа aim.mouse
inline void WriteAimCfg(std::ostringstream& out, const char* prefix, const Settings::AimbotConfig& c)
{
    auto k = [&](const char* name)
    {
        static char buf[128];
        std::snprintf(buf, sizeof(buf), "%s.%s", prefix, name);
        return buf;
    };

    PutBool(out, k("fov_enabled"), c.fov_enabled);
    PutFloat(out, k("fov_size"), c.fov_size);
    PutInt(out, k("fov_position"), c.fov_position);
    PutInt(out, k("fov_style"), c.fov_style);
    PutBool(out, k("fov_outline"), c.fov_outline);
    PutBool(out, k("distance_check"), c.distance_check);
    PutFloat(out, k("max_distance"), c.max_distance);
    PutBool(out, k("visible_only"), c.visible_only);
    PutBool(out, k("dead_check"), c.dead_check);
    PutInt(out, k("part_select"), c.part_select);
    PutInt(out, k("target_select"), c.target_select);
    PutFloat(out, k("switch_time"), c.switch_time);
    PutBool(out, k("hitchance_enabled"), c.hitchance_enabled);
    PutFloat(out, k("hitchance"), c.hitchance);
    PutFloat(out, k("smooth_x"), c.smooth_x);
    PutFloat(out, k("smooth_y"), c.smooth_y);
    PutBool(out, k("humanize"), c.humanize);
    PutFloat(out, k("reaction_ms"), c.reaction_ms);
    PutBool(out, k("sticky"), c.sticky);
    PutFloat(out, k("sticky_fov_scale"), c.sticky_fov_scale);
    PutBool(out, k("prediction"), c.prediction);
    PutFloat(out, k("bullet_speed"), c.bullet_speed);
    PutF4(out, k("fov_color"), c.fov_color);
    PutF4(out, k("fov_outline_color"), c.fov_outline_color);
    PutBool(out, k("tracer"), c.tracer);
    PutF4(out, k("tracer_color"), c.tracer_color);

    for (int i = 0; i < Settings::AIM_PART_COUNT; ++i)
    {
        char key[64];
        std::snprintf(key, sizeof(key), "%s.part_tier_%d", prefix, i);
        PutInt(out, key, c.part_tier[i]);
    }
}

inline void ReadAimCfg(const KV& kv, const char* prefix, Settings::AimbotConfig& c)
{
    auto k = [&](const char* name)
    {
        static char buf[128];
        std::snprintf(buf, sizeof(buf), "%s.%s", prefix, name);
        return buf;
    };

    GetBool(kv, k("fov_enabled"), c.fov_enabled);
    GetFloat(kv, k("fov_size"), c.fov_size);
    GetInt(kv, k("fov_position"), c.fov_position);
    GetInt(kv, k("fov_style"), c.fov_style);
    GetBool(kv, k("fov_outline"), c.fov_outline);
    GetBool(kv, k("distance_check"), c.distance_check);
    GetFloat(kv, k("max_distance"), c.max_distance);
    GetBool(kv, k("visible_only"), c.visible_only);
    GetBool(kv, k("dead_check"), c.dead_check);
    GetInt(kv, k("part_select"), c.part_select);
    GetInt(kv, k("target_select"), c.target_select);
    GetFloat(kv, k("switch_time"), c.switch_time);
    GetBool(kv, k("hitchance_enabled"), c.hitchance_enabled);
    GetFloat(kv, k("hitchance"), c.hitchance);
    GetFloat(kv, k("smooth_x"), c.smooth_x);
    GetFloat(kv, k("smooth_y"), c.smooth_y);
    GetBool(kv, k("humanize"), c.humanize);
    GetFloat(kv, k("reaction_ms"), c.reaction_ms);
    GetBool(kv, k("sticky"), c.sticky);
    GetFloat(kv, k("sticky_fov_scale"), c.sticky_fov_scale);
    GetBool(kv, k("prediction"), c.prediction);
    GetFloat(kv, k("bullet_speed"), c.bullet_speed);
    GetF4(kv, k("fov_color"), c.fov_color);
    GetF4(kv, k("fov_outline_color"), c.fov_outline_color);
    GetBool(kv, k("tracer"), c.tracer);
    GetF4(kv, k("tracer_color"), c.tracer_color);

    for (int i = 0; i < Settings::AIM_PART_COUNT; ++i)
    {
        char key[64];
        std::snprintf(key, sizeof(key), "%s.part_tier_%d", prefix, i);
        GetInt(kv, key, c.part_tier[i]);
    }
    c.SyncPartsFromTiers();
}

inline void WriteAim(std::ostringstream& out, const Settings& s)
{
    PutInt(out, "aim.bind", s.aim.bind);
    PutInt(out, "aim.bind_mode", s.aim.bind_mode);
    PutInt(out, "aim.type", s.aim.type);
    PutInt(out, "aim.silent_method", s.aim.silent_method);
    PutBool(out, "aim.silent_enabled", s.aim.silent_enabled);
    PutInt(out, "aim.silent_bind", s.aim.silent_bind);
    PutInt(out, "aim.silent_bind_mode", s.aim.silent_bind_mode);
    PutBool(out, "aim.force_magic_bullet", s.aim.force_magic_bullet);
    PutInt(out, "aim.force_magic_key", s.aim.force_magic_key);
    PutInt(out, "aim.force_magic_mode", s.aim.force_magic_mode);
    PutBool(out, "aim.target_bots", s.aim.target_bots);
    WriteAimCfg(out, "aim.mouse", s.aim.mouse);
    WriteAimCfg(out, "aim.camera", s.aim.camera);
    WriteAimCfg(out, "aim.silent", s.aim.silent);
}

inline void ReadAim(const KV& kv, Settings& s)
{
    GetInt(kv, "aim.bind", s.aim.bind);
    GetInt(kv, "aim.bind_mode", s.aim.bind_mode);
    GetInt(kv, "aim.type", s.aim.type);
    GetInt(kv, "aim.silent_method", s.aim.silent_method);
    GetBool(kv, "aim.force_magic_bullet", s.aim.force_magic_bullet);
    GetInt(kv, "aim.force_magic_key", s.aim.force_magic_key);
    GetInt(kv, "aim.force_magic_mode", s.aim.force_magic_mode);
    GetBool(kv, "aim.target_bots", s.aim.target_bots);
    ReadAimCfg(kv, "aim.mouse", s.aim.mouse);
    ReadAimCfg(kv, "aim.camera", s.aim.camera);
    ReadAimCfg(kv, "aim.silent", s.aim.silent);

	// старые конфиги: type==2 был silent, теперь silent_enabled + type off
	if (kv.count("aim.silent_enabled"))
	{
		GetBool(kv, "aim.silent_enabled", s.aim.silent_enabled);
	}

	else if (s.aim.type == 2)
	{
		s.aim.silent_enabled = true;
	}

	GetInt(kv, "aim.silent_bind", s.aim.silent_bind);
	GetInt(kv, "aim.silent_bind_mode", s.aim.silent_bind_mode);
}

} // namespace detail
} // namespace Config
} // namespace Cheat
