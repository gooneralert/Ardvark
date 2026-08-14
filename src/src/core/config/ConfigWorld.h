#pragma once

#include "ConfigIo.h"
#include "app/Settings.h"

namespace Cheat {
namespace Config {
namespace detail {

inline void WriteWorld(std::ostringstream& out, const Settings& s)
{
	PutBool(out, "world.no_shadow", s.world.no_shadow);
	PutBool(out, "world.time_changer", s.world.time_changer);
	PutFloat(out, "world.clock_time", s.world.clock_time);
	PutBool(out, "world.ambient", s.world.ambient);
	PutF4(out, "world.ambient_col", s.world.ambient_col);
	PutBool(out, "world.outdoor", s.world.outdoor);
	PutF4(out, "world.outdoor_col", s.world.outdoor_col);
	PutBool(out, "world.brightness", s.world.brightness);
	PutFloat(out, "world.brightness_val", s.world.brightness_val);
	PutBool(out, "world.exposure_on", s.world.exposure_on);
	PutFloat(out, "world.exposure", s.world.exposure);
	PutBool(out, "world.light", s.world.light);
	PutF4(out, "world.light_col", s.world.light_col);
	PutFloat(out, "world.light_dir_x", s.world.light_dir[0]);
	PutFloat(out, "world.light_dir_y", s.world.light_dir[1]);
	PutFloat(out, "world.light_dir_z", s.world.light_dir[2]);
	PutBool(out, "world.fog", s.world.fog);
	PutFloat(out, "world.fog_start", s.world.fog_start);
	PutFloat(out, "world.fog_end", s.world.fog_end);
	PutF4(out, "world.fog_color", s.world.fog_color);
	PutBool(out, "world.env", s.world.env);
	PutFloat(out, "world.env_diffuse", s.world.env_diffuse);
	PutFloat(out, "world.env_specular", s.world.env_specular);
	PutBool(out, "world.color_shift", s.world.color_shift);
	PutF4(out, "world.shift_top", s.world.shift_top);
	PutF4(out, "world.shift_bot", s.world.shift_bot);
	PutBool(out, "world.atmosphere", s.world.atmosphere);
	PutFloat(out, "world.atmo_density", s.world.atmo_density);
	PutFloat(out, "world.atmo_haze", s.world.atmo_haze);
	PutFloat(out, "world.atmo_glare", s.world.atmo_glare);
	PutFloat(out, "world.atmo_offset", s.world.atmo_offset);
	PutF4(out, "world.atmo_color", s.world.atmo_color);
	PutF4(out, "world.atmo_decay", s.world.atmo_decay);
	PutBool(out, "world.sky", s.world.sky);
	PutFloat(out, "world.sun_angular", s.world.sun_angular);
	PutFloat(out, "world.moon_angular", s.world.moon_angular);
	PutFloat(out, "world.sky_orient_x", s.world.sky_orient_xyz[0]);
	PutFloat(out, "world.sky_orient_y", s.world.sky_orient_xyz[1]);
	PutFloat(out, "world.sky_orient_z", s.world.sky_orient_xyz[2]);
	PutBool(out, "world.skybox_changer", s.world.skybox_changer);
	PutInt(out, "world.skybox_mode", s.world.skybox_mode);
	PutInt(out, "world.skybox_shader", s.world.skybox_shader);
	PutInt(out, "world.skybox_preset", s.world.skybox_preset);
	PutBool(out, "world.bloom", s.world.bloom);
	PutFloat(out, "world.bloom_intensity", s.world.bloom_intensity);
	PutFloat(out, "world.bloom_size", s.world.bloom_size);
	PutFloat(out, "world.bloom_threshold", s.world.bloom_threshold);
	PutBool(out, "world.color_corr", s.world.color_corr);
	PutFloat(out, "world.cc_bri", s.world.cc_bri);
	PutFloat(out, "world.cc_con", s.world.cc_con);
	PutF4(out, "world.cc_tint", s.world.cc_tint);
	PutBool(out, "world.color_grade", s.world.color_grade);
	PutInt(out, "world.tonemapper", s.world.tonemapper);
	PutBool(out, "world.dof", s.world.dof);
	PutFloat(out, "world.dof_far", s.world.dof_far);
	PutFloat(out, "world.dof_near", s.world.dof_near);
	PutFloat(out, "world.dof_focus", s.world.dof_focus);
	PutFloat(out, "world.dof_radius", s.world.dof_radius);
	PutBool(out, "world.terrain", s.world.terrain);
	PutFloat(out, "world.grass_len", s.world.grass_len);
	PutF4(out, "world.grass_col", s.world.grass_col);
	PutF4(out, "world.water_col", s.world.water_col);
	PutFloat(out, "world.water_refl", s.world.water_refl);
	PutFloat(out, "world.water_trans", s.world.water_trans);
}

inline void ReadWorld(const KV& kv, Settings& s)
{
	GetBool(kv, "world.no_shadow", s.world.no_shadow);
	GetBool(kv, "world.time_changer", s.world.time_changer);
	GetFloat(kv, "world.clock_time", s.world.clock_time);
	if (s.world.clock_time < 0.f) s.world.clock_time = 0.f;
	if (s.world.clock_time > 24.f) s.world.clock_time = 24.f;

	GetBool(kv, "world.ambient", s.world.ambient);
	GetF4(kv, "world.ambient_col", s.world.ambient_col);
	GetBool(kv, "world.outdoor", s.world.outdoor);
	GetF4(kv, "world.outdoor_col", s.world.outdoor_col);
	GetBool(kv, "world.brightness", s.world.brightness);
	GetFloat(kv, "world.brightness_val", s.world.brightness_val);
	GetBool(kv, "world.exposure_on", s.world.exposure_on);
	GetFloat(kv, "world.exposure", s.world.exposure);
	GetBool(kv, "world.light", s.world.light);
	GetF4(kv, "world.light_col", s.world.light_col);
	GetFloat(kv, "world.light_dir_x", s.world.light_dir[0]);
	GetFloat(kv, "world.light_dir_y", s.world.light_dir[1]);
	GetFloat(kv, "world.light_dir_z", s.world.light_dir[2]);

	// старый time_manual: если был manual — поднимем отдельные тогглы
	bool old_manual = false;
	GetBool(kv, "world.time_manual", old_manual);
	if (old_manual && s.world.time_changer)
	{
		if (!kv.count("world.ambient"))
			s.world.ambient = true;
		if (!kv.count("world.outdoor"))
			s.world.outdoor = true;
		if (!kv.count("world.brightness"))
			s.world.brightness = true;
		if (!kv.count("world.exposure_on"))
			s.world.exposure_on = true;
		if (!kv.count("world.light"))
			s.world.light = true;
	}

	GetBool(kv, "world.fog", s.world.fog);
	GetFloat(kv, "world.fog_start", s.world.fog_start);
	GetFloat(kv, "world.fog_end", s.world.fog_end);
	GetF4(kv, "world.fog_color", s.world.fog_color);
	GetBool(kv, "world.env", s.world.env);
	GetFloat(kv, "world.env_diffuse", s.world.env_diffuse);
	GetFloat(kv, "world.env_specular", s.world.env_specular);
	GetBool(kv, "world.color_shift", s.world.color_shift);
	GetF4(kv, "world.shift_top", s.world.shift_top);
	GetF4(kv, "world.shift_bot", s.world.shift_bot);
	GetBool(kv, "world.atmosphere", s.world.atmosphere);
	GetFloat(kv, "world.atmo_density", s.world.atmo_density);
	GetFloat(kv, "world.atmo_haze", s.world.atmo_haze);
	GetFloat(kv, "world.atmo_glare", s.world.atmo_glare);
	GetFloat(kv, "world.atmo_offset", s.world.atmo_offset);
	GetF4(kv, "world.atmo_color", s.world.atmo_color);
	GetF4(kv, "world.atmo_decay", s.world.atmo_decay);
	GetBool(kv, "world.sky", s.world.sky);
	GetFloat(kv, "world.sun_angular", s.world.sun_angular);
	GetFloat(kv, "world.moon_angular", s.world.moon_angular);
	GetFloat(kv, "world.sky_orient_x", s.world.sky_orient_xyz[0]);
	GetFloat(kv, "world.sky_orient_y", s.world.sky_orient_xyz[1]);
	GetFloat(kv, "world.sky_orient_z", s.world.sky_orient_xyz[2]);
	GetBool(kv, "world.skybox_changer", s.world.skybox_changer);
	GetInt(kv, "world.skybox_mode", s.world.skybox_mode);
	GetInt(kv, "world.skybox_shader", s.world.skybox_shader);
	GetInt(kv, "world.skybox_preset", s.world.skybox_preset);
	GetBool(kv, "world.bloom", s.world.bloom);
	GetFloat(kv, "world.bloom_intensity", s.world.bloom_intensity);
	GetFloat(kv, "world.bloom_size", s.world.bloom_size);
	GetFloat(kv, "world.bloom_threshold", s.world.bloom_threshold);
	GetBool(kv, "world.color_corr", s.world.color_corr);
	GetFloat(kv, "world.cc_bri", s.world.cc_bri);
	GetFloat(kv, "world.cc_con", s.world.cc_con);
	GetF4(kv, "world.cc_tint", s.world.cc_tint);
	GetBool(kv, "world.color_grade", s.world.color_grade);
	GetInt(kv, "world.tonemapper", s.world.tonemapper);
	GetBool(kv, "world.dof", s.world.dof);
	GetFloat(kv, "world.dof_far", s.world.dof_far);
	GetFloat(kv, "world.dof_near", s.world.dof_near);
	GetFloat(kv, "world.dof_focus", s.world.dof_focus);
	GetFloat(kv, "world.dof_radius", s.world.dof_radius);
	GetBool(kv, "world.terrain", s.world.terrain);
	GetFloat(kv, "world.grass_len", s.world.grass_len);
	GetF4(kv, "world.grass_col", s.world.grass_col);
	GetF4(kv, "world.water_col", s.world.water_col);
	GetFloat(kv, "world.water_refl", s.world.water_refl);
	GetFloat(kv, "world.water_trans", s.world.water_trans);
}

} // namespace detail
} // namespace Config
} // namespace Cheat
