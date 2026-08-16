#pragma once

#include "ConfigIo.h"
#include "app/Settings.h"

namespace Cheat {
namespace Config {
namespace detail {

inline void WriteCrosshair(std::ostringstream& out, const Settings& s)
{
    PutBool(out, "crosshair.enabled", s.crosshair.enabled);
    PutFloat(out, "crosshair.length", s.crosshair.length);
    PutFloat(out, "crosshair.gap", s.crosshair.gap);
    PutFloat(out, "crosshair.thickness", s.crosshair.thickness);
    PutBool(out, "crosshair.spin", s.crosshair.spin);
    PutFloat(out, "crosshair.spin_speed", s.crosshair.spin_speed);
    PutBool(out, "crosshair.outline", s.crosshair.outline);
    PutBool(out, "crosshair.dot", s.crosshair.dot);
    PutFloat(out, "crosshair.dot_size", s.crosshair.dot_size);
    PutF4(out, "crosshair.color", s.crosshair.color);
    PutF4(out, "crosshair.outline_color", s.crosshair.outline_color);
}

inline void ReadCrosshair(const KV& kv, Settings& s)
{
    GetBool(kv, "crosshair.enabled", s.crosshair.enabled);
    GetFloat(kv, "crosshair.length", s.crosshair.length);
    GetFloat(kv, "crosshair.gap", s.crosshair.gap);
    GetFloat(kv, "crosshair.thickness", s.crosshair.thickness);
    GetBool(kv, "crosshair.spin", s.crosshair.spin);
    GetFloat(kv, "crosshair.spin_speed", s.crosshair.spin_speed);
    GetBool(kv, "crosshair.outline", s.crosshair.outline);
    GetBool(kv, "crosshair.dot", s.crosshair.dot);
    GetFloat(kv, "crosshair.dot_size", s.crosshair.dot_size);
    GetF4(kv, "crosshair.color", s.crosshair.color);
    GetF4(kv, "crosshair.outline_color", s.crosshair.outline_color);
}

inline void WriteHitbox(std::ostringstream& out, const Settings& s)
{
    PutBool(out, "hitbox.enabled", s.hitbox.enabled);
    PutBool(out, "hitbox.visualize", s.hitbox.visualize);
    PutInt(out, "hitbox.viz_mode", s.hitbox.viz_mode);
    PutF4(out, "hitbox.viz_color", s.hitbox.viz_color);
    PutInt(out, "hitbox.part", s.hitbox.part);
    PutFloat(out, "hitbox.scale", s.hitbox.scale);
}

inline void ReadHitbox(const KV& kv, Settings& s)
{
    GetBool(kv, "hitbox.enabled", s.hitbox.enabled);
    GetBool(kv, "hitbox.visualize", s.hitbox.visualize);
    GetInt(kv, "hitbox.viz_mode", s.hitbox.viz_mode);
    GetF4(kv, "hitbox.viz_color", s.hitbox.viz_color);
    GetInt(kv, "hitbox.part", s.hitbox.part);
    GetFloat(kv, "hitbox.scale", s.hitbox.scale);
    if (s.hitbox.viz_mode < 0)
    {
        s.hitbox.viz_mode = 0;
    }

    if (s.hitbox.viz_mode > 2)
    {
        s.hitbox.viz_mode = 2;
    }

    if (s.hitbox.part < 0)
    {
        s.hitbox.part = 0;
    }

    if (s.hitbox.part >= Settings::HB_PART_COUNT)
    {
        s.hitbox.part = Settings::HB_HEAD;
    }
}

inline void WriteMisc(std::ostringstream& out, const Settings& s)
{
    PutBool(out, "misc.fps_unlock", s.misc.fps_unlock);
    PutInt(out, "misc.fps_cap", s.misc.fps_cap);
    PutBool(out, "misc.fov", s.misc.fov);
    PutFloat(out, "misc.fov_value", s.misc.fov_value);
    PutBool(out, "misc.walkspeed", s.misc.walkspeed);
    PutInt(out, "misc.walkspeed_mode", s.misc.walkspeed_mode);
    PutFloat(out, "misc.walkspeed_value", s.misc.walkspeed_value);
    PutBool(out, "misc.jump", s.misc.jump);
    PutFloat(out, "misc.jump_power", s.misc.jump_power);
    PutBool(out, "misc.fly", s.misc.fly);
    PutFloat(out, "misc.fly_speed", s.misc.fly_speed);
    PutInt(out, "misc.fly_key", s.misc.fly_key);
    PutInt(out, "misc.fly_key_mode", s.misc.fly_key_mode);
    PutBool(out, "misc.fly_gravity", s.misc.fly_gravity);
    PutBool(out, "misc.gravity", s.misc.gravity);
    PutFloat(out, "misc.gravity_value", s.misc.gravity_value);
    PutBool(out, "misc.tickrate", s.misc.tickrate);
    PutFloat(out, "misc.tickrate_value", s.misc.tickrate_value);
    PutBool(out, "misc.anim_changer", s.misc.anim_changer);
    PutInt(out, "misc.anim_pack", s.misc.anim_pack);
    PutBool(out, "misc.fake_headless", s.misc.fake_headless);
    PutBool(out, "misc.korblox", s.misc.korblox);
    PutBool(out, "misc.third_person", s.misc.third_person);
    PutInt(out, "misc.third_person_key", s.misc.third_person_key);
    PutInt(out, "misc.third_person_mode", s.misc.third_person_mode);
    PutFloat(out, "misc.third_person_distance", s.misc.third_person_distance);
    PutBool(out, "misc.noclip", s.misc.noclip);
    PutBool(out, "misc.inf_jump", s.misc.inf_jump);
    PutBool(out, "misc.btools", s.misc.bTools);
    PutInt(out, "misc.btools_tool", s.misc.bToolsTool);
    PutBool(out, "misc.teamcheck", s.misc.teamcheck);
    PutBool(out, "misc.raycast_engine", s.misc.raycast_engine);
    PutBool(out, "misc.explorer", s.misc.explorer);
    PutBool(out, "misc.esp_preview", s.misc.esp_preview);
    PutBool(out, "misc.mcp", s.misc.mcp);
    PutBool(out, "lua.executor", s.lua.executor);
    PutFloat(out, "lua.ticks_ms", s.lua.ticks_ms);
}

inline void ReadMisc(const KV& kv, Settings& s)
{
    GetBool(kv, "misc.fps_unlock", s.misc.fps_unlock);
    GetInt(kv, "misc.fps_cap", s.misc.fps_cap);
    GetBool(kv, "misc.fov", s.misc.fov);
    GetFloat(kv, "misc.fov_value", s.misc.fov_value);
    GetBool(kv, "misc.walkspeed", s.misc.walkspeed);
    GetInt(kv, "misc.walkspeed_mode", s.misc.walkspeed_mode);
    GetFloat(kv, "misc.walkspeed_value", s.misc.walkspeed_value);
    GetBool(kv, "misc.jump", s.misc.jump);
    GetFloat(kv, "misc.jump_power", s.misc.jump_power);
    GetBool(kv, "misc.fly", s.misc.fly);
    GetFloat(kv, "misc.fly_speed", s.misc.fly_speed);
    GetInt(kv, "misc.fly_key", s.misc.fly_key);
    GetInt(kv, "misc.fly_key_mode", s.misc.fly_key_mode);
    GetBool(kv, "misc.fly_gravity", s.misc.fly_gravity);
    GetBool(kv, "misc.gravity", s.misc.gravity);
    GetFloat(kv, "misc.gravity_value", s.misc.gravity_value);
    GetBool(kv, "misc.tickrate", s.misc.tickrate);
    GetFloat(kv, "misc.tickrate_value", s.misc.tickrate_value);
    GetBool(kv, "misc.anim_changer", s.misc.anim_changer);
    GetInt(kv, "misc.anim_pack", s.misc.anim_pack);
    GetBool(kv, "misc.fake_headless", s.misc.fake_headless);
    GetBool(kv, "misc.korblox", s.misc.korblox);
    GetBool(kv, "misc.third_person", s.misc.third_person);
    GetInt(kv, "misc.third_person_key", s.misc.third_person_key);
    GetInt(kv, "misc.third_person_mode", s.misc.third_person_mode);
    GetFloat(kv, "misc.third_person_distance", s.misc.third_person_distance);
    GetBool(kv, "misc.noclip", s.misc.noclip);
    GetBool(kv, "misc.inf_jump", s.misc.inf_jump);
    GetBool(kv, "misc.btools", s.misc.bTools);
    GetInt(kv, "misc.btools_tool", s.misc.bToolsTool);
    GetBool(kv, "misc.teamcheck", s.misc.teamcheck);
    GetBool(kv, "misc.raycast_engine", s.misc.raycast_engine);
    GetBool(kv, "misc.explorer", s.misc.explorer);
    GetBool(kv, "misc.esp_preview", s.misc.esp_preview);
    GetBool(kv, "misc.mcp", s.misc.mcp);
    GetBool(kv, "lua.executor", s.lua.executor);
    GetFloat(kv, "lua.ticks_ms", s.lua.ticks_ms);
    if (s.lua.ticks_ms < 1.f) s.lua.ticks_ms = 1.f;
    if (s.lua.ticks_ms > 15.f) s.lua.ticks_ms = 15.f;
}

inline void WriteHits(std::ostringstream& out, const Settings& s)
{
    PutBool(out, "killfx.enabled", s.killfx.enabled);
    PutInt(out, "killfx.effect", s.killfx.effect);
    PutBool(out, "hitmarker.enabled", s.hitmarker.enabled);
    PutFloat(out, "hitmarker.size", s.hitmarker.size);
    PutFloat(out, "hitmarker.duration", s.hitmarker.duration);
    PutBool(out, "hitsound.enabled", s.hitsound.enabled);
    PutInt(out, "hitsound.index", s.hitsound.index);
    PutFloat(out, "hitsound.volume", s.hitsound.volume);
    PutBool(out, "hitdata.enabled", s.hitdata.enabled);
}

inline void ReadHits(const KV& kv, Settings& s)
{
    GetBool(kv, "killfx.enabled", s.killfx.enabled);
    GetInt(kv, "killfx.effect", s.killfx.effect);
    GetBool(kv, "hitmarker.enabled", s.hitmarker.enabled);
    GetFloat(kv, "hitmarker.size", s.hitmarker.size);
    GetFloat(kv, "hitmarker.duration", s.hitmarker.duration);
    GetBool(kv, "hitsound.enabled", s.hitsound.enabled);
    GetInt(kv, "hitsound.index", s.hitsound.index);
    GetFloat(kv, "hitsound.volume", s.hitsound.volume);
    GetBool(kv, "hitdata.enabled", s.hitdata.enabled);
}

} // namespace detail
} // namespace Config
} // namespace Cheat
