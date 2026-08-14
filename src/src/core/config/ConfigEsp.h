#pragma once

#include "ConfigIo.h"
#include "app/Settings.h"

namespace Cheat {
namespace Config {
namespace detail {

inline void WriteEsp(std::ostringstream& out, const Settings& s)
{
    PutBool(out, "esp.enabled", s.esp.enabled);
    PutBool(out, "esp.draw_local", s.esp.draw_local);
    PutBool(out, "esp.preview", s.esp.preview);
    PutBool(out, "esp.box", s.esp.box);
    PutBool(out, "esp.box_fill", s.esp.box_fill);
    PutInt(out, "esp.box_fill_mode", s.esp.box_fill_mode);
    PutInt(out, "esp.box_fill_image", s.esp.box_fill_image);
    PutFloat(out, "esp.box_fill_image_alpha", s.esp.box_fill_image_alpha);
    PutBool(out, "esp.box_fill_remove_bg", s.esp.box_fill_remove_bg);
    PutF4(out, "esp.box_fill_color", s.esp.box_fill_color);
    PutBool(out, "esp.name", s.esp.name);
    PutBool(out, "esp.skeleton", s.esp.skeleton);
    PutInt(out, "esp.skeleton_type", s.esp.skeleton_type);
    PutBool(out, "esp.chams", s.esp.chams);
    PutInt(out, "esp.chams_mode", s.esp.chams_mode);
    PutBool(out, "esp.engine_chams", s.esp.engine_chams);
    PutInt(out, "esp.chams_shader", s.esp.chams_shader);
    PutInt(out, "esp.mesh_chams_style", s.esp.mesh_chams_style);
    PutInt(out, "esp.mesh_chams_dx_mode", s.esp.mesh_chams_dx_mode);
    PutBool(out, "esp.mesh_chams_occlusion", s.esp.mesh_chams_occlusion);
    PutInt(out, "esp.mesh_chams_occluded_dx_mode", s.esp.mesh_chams_occluded_dx_mode);
    PutF4(out, "esp.mesh_chams_occluded_color", s.esp.mesh_chams_occluded_color);
    PutF4(out, "esp.mesh_chams_occluded_outline_color", s.esp.mesh_chams_occluded_outline_color);
    PutBool(out, "esp.mesh_chams_outline", s.esp.mesh_chams_outline);
    PutInt(out, "esp.mesh_chams_outline_style", s.esp.mesh_chams_outline_style);
    PutFloat(out, "esp.mesh_chams_outline_fade", s.esp.mesh_chams_outline_fade);
    PutF4(out, "esp.mesh_chams_outline_color", s.esp.mesh_chams_outline_color);
    PutInt(out, "esp.engine_chams_style", s.esp.engine_chams_style);
    PutInt(out, "esp.engine_ghost_color_idx", s.esp.engine_ghost_color_idx);
    PutF4(out, "esp.engine_chams_color", s.esp.engine_chams_color);
    PutBool(out, "esp.healthbar", s.esp.healthbar);
    PutBool(out, "esp.health_text", s.esp.health_text);
    PutBool(out, "esp.distance", s.esp.distance);
    PutBool(out, "esp.tool", s.esp.tool);
    PutBool(out, "esp.flags", s.esp.flags);
    PutBool(out, "esp.tracer", s.esp.tracer);
    PutInt(out, "esp.tracer_origin", s.esp.tracer_origin);
    PutBool(out, "esp.china_hat", s.esp.china_hat);
    PutF4(out, "esp.china_hat_color", s.esp.china_hat_color);
    PutFloat(out, "esp.china_hat_height", s.esp.china_hat_height);
    PutFloat(out, "esp.china_hat_radius", s.esp.china_hat_radius);
    PutBool(out, "esp.hit_chams", s.esp.hit_chams);
    PutF4(out, "esp.hit_chams_color", s.esp.hit_chams_color);
    PutFloat(out, "esp.hit_chams_duration", s.esp.hit_chams_duration);
    PutBool(out, "esp.offscreen_arrows", s.esp.offscreen_arrows);
    PutFloat(out, "esp.arrow_size", s.esp.arrow_size);
    PutFloat(out, "esp.arrow_radius", s.esp.arrow_radius);
    for (int i = 0; i < Settings::ARROW_INFO_COUNT; ++i) {
        char key[48];
        std::snprintf(key, sizeof(key), "esp.arrow_info_%d", i);
        PutBool(out, key, s.esp.arrow_info[i]);
    }
    PutInt(out, "esp.font", s.esp.font);
    PutFloat(out, "esp.font_size", s.esp.font_size);
    PutInt(out, "esp.box_mode", s.esp.box_mode);
    PutInt(out, "esp.bounding_type", s.esp.bounding_type);
    for (int i = 0; i < Settings::ESP_OUTLINE_COUNT; ++i) {
        char key[48];
        std::snprintf(key, sizeof(key), "esp.esp_outline_%d", i);
        PutBool(out, key, s.esp.esp_outline[i]);
    }
    PutFloat(out, "esp.skeleton_thickness", s.esp.skeleton_thickness);
    PutFloat(out, "esp.box_thickness", s.esp.box_thickness);
    PutInt(out, "esp.name_mode", s.esp.name_mode);
    PutInt(out, "esp.distance_unit", s.esp.distance_unit);
    PutBool(out, "esp.distance_check", s.esp.distance_check);
    PutFloat(out, "esp.max_distance", s.esp.max_distance);
    PutBool(out, "esp.dead_check", s.esp.dead_check);
    PutBool(out, "esp.body_corpse", s.esp.body_corpse);
    PutF4(out, "esp.corpse_color", s.esp.corpse_color);
    PutBool(out, "esp.bots", s.esp.bots);
    PutFloat(out, "esp.bot_max_distance", s.esp.bot_max_distance);
    for (int i = 0; i < Settings::BOT_ESP_COUNT; ++i) {
        char key[48];
        std::snprintf(key, sizeof(key), "esp.bot_esp_%d", i);
        PutBool(out, key, s.esp.bot_esp[i]);
    }
    PutInt(out, "esp.bot_chams_mode", s.esp.bot_chams_mode);
    PutInt(out, "esp.bot_chams_shader", s.esp.bot_chams_shader);
    PutF4(out, "esp.bot_box_color", s.esp.bot_box_color);
    PutF4(out, "esp.bot_name_color", s.esp.bot_name_color);
    PutF4(out, "esp.bot_skeleton_color", s.esp.bot_skeleton_color);
    PutF4(out, "esp.bot_chams_outline_color", s.esp.bot_chams_outline_color);
    PutF4(out, "esp.bot_chams_fill_color", s.esp.bot_chams_fill_color);
    PutF4(out, "esp.bot_distance_color", s.esp.bot_distance_color);
    PutF4(out, "esp.bot_tool_color", s.esp.bot_tool_color);
    PutBool(out, "esp.corpses", s.esp.corpses);
    PutBool(out, "esp.ground_loot", s.esp.ground_loot);
    PutBool(out, "esp.containers", s.esp.containers);
    PutF4(out, "esp.ground_loot_color", s.esp.ground_loot_color);
    PutF4(out, "esp.containers_color", s.esp.containers_color);
    PutBool(out, "esp.loot_chams", s.esp.loot_chams);
    PutBool(out, "esp.containers_chams", s.esp.containers_chams);
    PutInt(out, "esp.loot_chams_shader", s.esp.loot_chams_shader);
    PutInt(out, "esp.containers_chams_shader", s.esp.containers_chams_shader);
    {
        static const char* k_loot_keys[] = {
            "esp.loot_weapons", "esp.loot_mags", "esp.loot_ammo", "esp.loot_attachments",
            "esp.loot_medical", "esp.loot_valuables", "esp.loot_tools", "esp.loot_electronics",
            "esp.loot_households", "esp.loot_documents", "esp.loot_other"
        };
        for (int i = 0; i < Settings::LOOT_FILTER_COUNT; ++i)
            PutBool(out, k_loot_keys[i], s.esp.loot_filter[i]);
    }
    PutInt(out, "esp.name_side", s.esp.name_side);
    PutFloat(out, "esp.name_off", s.esp.name_off);
    PutInt(out, "esp.distance_side", s.esp.distance_side);
    PutFloat(out, "esp.distance_off", s.esp.distance_off);
    PutInt(out, "esp.tool_side", s.esp.tool_side);
    PutFloat(out, "esp.tool_off", s.esp.tool_off);
    PutInt(out, "esp.flags_side", s.esp.flags_side);
    PutFloat(out, "esp.flags_off", s.esp.flags_off);
    PutInt(out, "esp.health_text_side", s.esp.health_text_side);
    PutFloat(out, "esp.health_text_off", s.esp.health_text_off);
    PutInt(out, "esp.healthbar_side", s.esp.healthbar_side);
    PutF4(out, "esp.box_color", s.esp.box_color);
    PutF4(out, "esp.name_color", s.esp.name_color);
    PutF4(out, "esp.skeleton_color", s.esp.skeleton_color);
    PutF4(out, "esp.chams_outline_color", s.esp.chams_outline_color);
    PutF4(out, "esp.chams_fill_color", s.esp.chams_fill_color);
    PutF4(out, "esp.distance_color", s.esp.distance_color);
    PutF4(out, "esp.tool_color", s.esp.tool_color);
    PutF4(out, "esp.tracer_color", s.esp.tracer_color);
    PutF4(out, "esp.arrow_color", s.esp.arrow_color);
}

inline void ReadEsp(const KV& kv, Settings& s)
{
    GetBool(kv, "esp.enabled", s.esp.enabled);
    GetBool(kv, "esp.draw_local", s.esp.draw_local);
    GetBool(kv, "esp.preview", s.esp.preview);
    GetBool(kv, "esp.box", s.esp.box);
    GetBool(kv, "esp.box_fill", s.esp.box_fill);
    GetInt(kv, "esp.box_fill_mode", s.esp.box_fill_mode);
    GetInt(kv, "esp.box_fill_image", s.esp.box_fill_image);
    GetFloat(kv, "esp.box_fill_image_alpha", s.esp.box_fill_image_alpha);
    GetBool(kv, "esp.box_fill_remove_bg", s.esp.box_fill_remove_bg);
    GetF4(kv, "esp.box_fill_color", s.esp.box_fill_color);
    GetBool(kv, "esp.name", s.esp.name);
    GetBool(kv, "esp.skeleton", s.esp.skeleton);
    GetInt(kv, "esp.skeleton_type", s.esp.skeleton_type);
    GetBool(kv, "esp.chams", s.esp.chams);
    GetInt(kv, "esp.chams_mode", s.esp.chams_mode);
    const bool has_engine_key = GetBool(kv, "esp.engine_chams", s.esp.engine_chams);
    // legacy без ключа: mode4=engine, mode5=mesh
    if (!has_engine_key && s.esp.chams_mode == 4)
    {
        s.esp.engine_chams = true;
        s.esp.chams_mode = 0;
    }
    if (s.esp.chams_mode == 5)
    {
        s.esp.chams_mode = 4;
    }
    if (s.esp.chams_mode < 0)
    {
        s.esp.chams_mode = 0;
    }
    if (s.esp.chams_mode > 4)
    {
        s.esp.chams_mode = 4;
    }
    GetInt(kv, "esp.chams_shader", s.esp.chams_shader);
    GetInt(kv, "esp.mesh_chams_style", s.esp.mesh_chams_style);
    GetInt(kv, "esp.mesh_chams_dx_mode", s.esp.mesh_chams_dx_mode);
    GetBool(kv, "esp.mesh_chams_occlusion", s.esp.mesh_chams_occlusion);
    GetInt(kv, "esp.mesh_chams_occluded_dx_mode", s.esp.mesh_chams_occluded_dx_mode);
    GetF4(kv, "esp.mesh_chams_occluded_color", s.esp.mesh_chams_occluded_color);
    GetF4(kv, "esp.mesh_chams_occluded_outline_color", s.esp.mesh_chams_occluded_outline_color);
    GetBool(kv, "esp.mesh_chams_outline", s.esp.mesh_chams_outline);
    GetInt(kv, "esp.mesh_chams_outline_style", s.esp.mesh_chams_outline_style);
    GetFloat(kv, "esp.mesh_chams_outline_fade", s.esp.mesh_chams_outline_fade);
    GetF4(kv, "esp.mesh_chams_outline_color", s.esp.mesh_chams_outline_color);
    GetInt(kv, "esp.engine_chams_style", s.esp.engine_chams_style);
    GetInt(kv, "esp.engine_ghost_color_idx", s.esp.engine_ghost_color_idx);
    GetF4(kv, "esp.engine_chams_color", s.esp.engine_chams_color);
    if (s.esp.engine_ghost_color_idx < 0 || s.esp.engine_ghost_color_idx > 6)
    {
        s.esp.engine_ghost_color_idx = 6;
    }
    if (s.esp.engine_chams_style < 0)
    {
        s.esp.engine_chams_style = 0;
    }

    if (s.esp.engine_chams_style > 7)
    {
        s.esp.engine_chams_style = 7;
    }
    GetBool(kv, "esp.healthbar", s.esp.healthbar);
    GetBool(kv, "esp.health_text", s.esp.health_text);
    GetBool(kv, "esp.distance", s.esp.distance);
    GetBool(kv, "esp.tool", s.esp.tool);
    GetBool(kv, "esp.flags", s.esp.flags);
    GetBool(kv, "esp.tracer", s.esp.tracer);
    GetInt(kv, "esp.tracer_origin", s.esp.tracer_origin);
    GetBool(kv, "esp.china_hat", s.esp.china_hat);
    GetF4(kv, "esp.china_hat_color", s.esp.china_hat_color);
    GetFloat(kv, "esp.china_hat_height", s.esp.china_hat_height);
    GetFloat(kv, "esp.china_hat_radius", s.esp.china_hat_radius);
    GetBool(kv, "esp.hit_chams", s.esp.hit_chams);
    GetF4(kv, "esp.hit_chams_color", s.esp.hit_chams_color);
    GetFloat(kv, "esp.hit_chams_duration", s.esp.hit_chams_duration);
    GetBool(kv, "esp.offscreen_arrows", s.esp.offscreen_arrows);
    GetFloat(kv, "esp.arrow_size", s.esp.arrow_size);
    GetFloat(kv, "esp.arrow_radius", s.esp.arrow_radius);
    for (int i = 0; i < Settings::ARROW_INFO_COUNT; ++i) {
        char key[48];
        std::snprintf(key, sizeof(key), "esp.arrow_info_%d", i);
        GetBool(kv, key, s.esp.arrow_info[i]);
    }
    GetInt(kv, "esp.font", s.esp.font);
    GetFloat(kv, "esp.font_size", s.esp.font_size);
    GetInt(kv, "esp.box_mode", s.esp.box_mode);
    GetInt(kv, "esp.bounding_type", s.esp.bounding_type);
    for (int i = 0; i < Settings::ESP_OUTLINE_COUNT; ++i) {
        char key[48];
        std::snprintf(key, sizeof(key), "esp.esp_outline_%d", i);
        GetBool(kv, key, s.esp.esp_outline[i]);
    }
    GetFloat(kv, "esp.skeleton_thickness", s.esp.skeleton_thickness);
    if (s.esp.skeleton_thickness < 1.f) s.esp.skeleton_thickness = 1.f;
    GetFloat(kv, "esp.box_thickness", s.esp.box_thickness);
    GetInt(kv, "esp.name_mode", s.esp.name_mode);
    GetInt(kv, "esp.distance_unit", s.esp.distance_unit);
    GetBool(kv, "esp.distance_check", s.esp.distance_check);
    GetFloat(kv, "esp.max_distance", s.esp.max_distance);
    GetBool(kv, "esp.dead_check", s.esp.dead_check);
    GetBool(kv, "esp.body_corpse", s.esp.body_corpse);
    GetF4(kv, "esp.corpse_color", s.esp.corpse_color);
    GetBool(kv, "esp.bots", s.esp.bots);
    GetFloat(kv, "esp.bot_max_distance", s.esp.bot_max_distance);
    for (int i = 0; i < Settings::BOT_ESP_COUNT; ++i) {
        char key[48];
        std::snprintf(key, sizeof(key), "esp.bot_esp_%d", i);
        GetBool(kv, key, s.esp.bot_esp[i]);
    }
    GetInt(kv, "esp.bot_chams_mode", s.esp.bot_chams_mode);
    GetInt(kv, "esp.bot_chams_shader", s.esp.bot_chams_shader);
    GetF4(kv, "esp.bot_box_color", s.esp.bot_box_color);
    GetF4(kv, "esp.bot_name_color", s.esp.bot_name_color);
    GetF4(kv, "esp.bot_skeleton_color", s.esp.bot_skeleton_color);
    GetF4(kv, "esp.bot_chams_outline_color", s.esp.bot_chams_outline_color);
    GetF4(kv, "esp.bot_chams_fill_color", s.esp.bot_chams_fill_color);
    GetF4(kv, "esp.bot_distance_color", s.esp.bot_distance_color);
    GetF4(kv, "esp.bot_tool_color", s.esp.bot_tool_color);
    GetBool(kv, "esp.corpses", s.esp.corpses);
    GetBool(kv, "esp.ground_loot", s.esp.ground_loot);
    GetBool(kv, "esp.containers", s.esp.containers);
    GetF4(kv, "esp.ground_loot_color", s.esp.ground_loot_color);
    GetF4(kv, "esp.containers_color", s.esp.containers_color);
    GetBool(kv, "esp.loot_chams", s.esp.loot_chams);
    GetBool(kv, "esp.containers_chams", s.esp.containers_chams);
    GetInt(kv, "esp.loot_chams_shader", s.esp.loot_chams_shader);
    GetInt(kv, "esp.containers_chams_shader", s.esp.containers_chams_shader);
    {
        static const char* k_loot_keys[] = {
            "esp.loot_weapons", "esp.loot_mags", "esp.loot_ammo", "esp.loot_attachments",
            "esp.loot_medical", "esp.loot_valuables", "esp.loot_tools", "esp.loot_electronics",
            "esp.loot_households", "esp.loot_documents", "esp.loot_other"
        };
        for (int i = 0; i < Settings::LOOT_FILTER_COUNT; ++i)
            GetBool(kv, k_loot_keys[i], s.esp.loot_filter[i]);
    }
    GetInt(kv, "esp.name_side", s.esp.name_side);
    GetFloat(kv, "esp.name_off", s.esp.name_off);
    GetInt(kv, "esp.distance_side", s.esp.distance_side);
    GetFloat(kv, "esp.distance_off", s.esp.distance_off);
    GetInt(kv, "esp.tool_side", s.esp.tool_side);
    GetFloat(kv, "esp.tool_off", s.esp.tool_off);
    GetInt(kv, "esp.flags_side", s.esp.flags_side);
    GetFloat(kv, "esp.flags_off", s.esp.flags_off);
    GetInt(kv, "esp.health_text_side", s.esp.health_text_side);
    GetFloat(kv, "esp.health_text_off", s.esp.health_text_off);
    GetInt(kv, "esp.healthbar_side", s.esp.healthbar_side);
    GetF4(kv, "esp.box_color", s.esp.box_color);
    GetF4(kv, "esp.name_color", s.esp.name_color);
    GetF4(kv, "esp.skeleton_color", s.esp.skeleton_color);
    GetF4(kv, "esp.chams_outline_color", s.esp.chams_outline_color);
    GetF4(kv, "esp.chams_fill_color", s.esp.chams_fill_color);
    GetF4(kv, "esp.distance_color", s.esp.distance_color);
    GetF4(kv, "esp.tool_color", s.esp.tool_color);
    GetF4(kv, "esp.tracer_color", s.esp.tracer_color);
    GetF4(kv, "esp.arrow_color", s.esp.arrow_color);
}

} // namespace detail
} // namespace Config
} // namespace Cheat
