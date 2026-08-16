#include "pch.h"
#include "Config.h"
#include "app/Settings.h"

#include <Windows.h>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace Cheat {
namespace Config {
namespace {

// имя файла без мусора
std::string SanitizeName(std::string name)
{
    for (char& c : name)
    {
        bool ok =
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-' || c == ' ';
        if (!ok)
            c = '_';
    }
    while (!name.empty() && (name.front() == ' ' || name.front() == '.'))
        name.erase(name.begin());
    while (!name.empty() && (name.back() == ' ' || name.back() == '.'))
        name.pop_back();
    if (name.empty())
        name = "config";
    // слишком длинное, обрежем
    if (name.size() > 48)
        name.resize(48);
    return name;
}

std::string PathFor(const std::string& name)
{
    return Directory() + "\\" + SanitizeName(name) + ".cfg";
}

void EnsureDir(const std::string& dir)
{
    CreateDirectoryA(dir.c_str(), nullptr);
}

using KV = std::unordered_map<std::string, std::string>;

void PutBool(std::ostringstream& out, const char* key, bool v)
{
    out << key << '=' << (v ? 1 : 0) << '\n';
}

void PutInt(std::ostringstream& out, const char* key, int v)
{
    out << key << '=' << v << '\n';
}

void PutFloat(std::ostringstream& out, const char* key, float v)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.6g", v);
    out << key << '=' << buf << '\n';
}

void PutF4(std::ostringstream& out, const char* key, const float c[4])
{
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%.6g,%.6g,%.6g,%.6g", c[0], c[1], c[2], c[3]);
    out << key << '=' << buf << '\n';
}

void PutF2(std::ostringstream& out, const char* key, const float c[2])
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.6g,%.6g", c[0], c[1]);
    out << key << '=' << buf << '\n';
}

bool GetBool(const KV& kv, const char* key, bool& out)
{
    auto it = kv.find(key);
    if (it == kv.end())
        return false;
    out = (it->second != "0" && it->second != "false");
    return true;
}

bool GetInt(const KV& kv, const char* key, int& out)
{
    auto it = kv.find(key);
    if (it == kv.end())
        return false;
    out = std::atoi(it->second.c_str());
    return true;
}

// legacy 0..13 → базовый набор
int RemapMeshDxMode(int old)
{
    switch (old)
    {
    case 0:  return 0; // flat
    case 4:  return 1; // chrome
    case 5:  return 2; // rainbow
    case 8:  return 1; // metal → chrome
    case 10: return 3; // glass → pearl
    case 11: return 3; // pearl
    case 12: return 4; // glossy
    case 13: return 5; // holographic
    default:
        if (old >= 0 && old <= 5)
            return 0;
        return 0;
    }
}

// v2 имел glow@8; сейчас: glass@8 ropes@9 liquid@10
int CompactMeshDxModeV2(int m)
{
    if (m < 0) return 0;
    if (m <= 7) return m;
    if (m == 8) return 0;  // glow удалён
    if (m == 9) return 8;  // glass
    if (m == 10) return 9; // ropes
    if (m == 11) return 10; // liquid metal
    return 10;
}

void ClampMeshDxMode(int& out)
{
    if (out < 0) out = 0;
    if (out > 22) out = 22;
}

void LoadMeshDxMode(const KV& kv, int& out)
{
    if (GetInt(kv, "esp.mesh_chams_dx_mode_v3", out))
    {
        ClampMeshDxMode(out);
        return;
    }
    if (GetInt(kv, "esp.mesh_chams_dx_mode_v2", out))
    {
        out = CompactMeshDxModeV2(out);
        ClampMeshDxMode(out);
        return;
    }
    int legacy = out;
    if (GetInt(kv, "esp.mesh_chams_dx_mode", legacy))
        out = RemapMeshDxMode(legacy);
    ClampMeshDxMode(out);
}

void LoadMeshDxOccludedMode(const KV& kv, int& out)
{
    if (GetInt(kv, "esp.mesh_chams_occluded_dx_mode_v3", out))
    {
        ClampMeshDxMode(out);
        return;
    }
    if (GetInt(kv, "esp.mesh_chams_occluded_dx_mode_v2", out))
    {
        out = CompactMeshDxModeV2(out);
        ClampMeshDxMode(out);
        return;
    }
    int legacy = out;
    if (GetInt(kv, "esp.mesh_chams_occluded_dx_mode", legacy))
        out = RemapMeshDxMode(legacy);
    ClampMeshDxMode(out);
}

bool GetFloat(const KV& kv, const char* key, float& out)
{
    auto it = kv.find(key);
    if (it == kv.end())
        return false;
    out = (float)std::atof(it->second.c_str());
    return true;
}

bool GetF4(const KV& kv, const char* key, float out[4])
{
    auto it = kv.find(key);
    if (it == kv.end())
        return false;

    float a = out[0], b = out[1], c = out[2], d = out[3];
    if (sscanf_s(it->second.c_str(), "%f,%f,%f,%f", &a, &b, &c, &d) == 4)
    {
        out[0] = a;
        out[1] = b;
        out[2] = c;
        out[3] = d;
        return true;
    }
    return false;
}

bool GetF2(const KV& kv, const char* key, float out[2])
{
    auto it = kv.find(key);
    if (it == kv.end())
        return false;

    float a = out[0], b = out[1];
    if (sscanf_s(it->second.c_str(), "%f,%f", &a, &b) == 2)
    {
        out[0] = a;
        out[1] = b;
        return true;
    }
    return false;
}

// aimbot блок, префикс типа aim.mouse
void WriteAimCfg(std::ostringstream& out, const char* prefix, const Settings::AimbotConfig& c)
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

void ReadAimCfg(const KV& kv, const char* prefix, Settings::AimbotConfig& c)
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

} // namespace

std::string Directory()
{
    char module[MAX_PATH]{};
    GetModuleFileNameA(nullptr, module, MAX_PATH);
    std::string path(module);
    auto slash = path.find_last_of("\\/");
    if (slash != std::string::npos)
        path.resize(slash);
    path += "\\configs";
    return path;
}

std::vector<std::string> List()
{
    std::vector<std::string> out;
    std::string dir = Directory();
    EnsureDir(dir);

    WIN32_FIND_DATAA fd{};
    std::string pattern = dir + "\\*.cfg";
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE)
        return out;

    do
    {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        std::string name = fd.cFileName;
        // срезаем .cfg
        if (name.size() > 4 && name.substr(name.size() - 4) == ".cfg")
            name.resize(name.size() - 4);
        out.push_back(name);
    }
    while (FindNextFileA(h, &fd));

    FindClose(h);
    std::sort(out.begin(), out.end());
    return out;
}

// сейв всего g_Settings в .cfg
bool Save(const std::string& name)
{
    const std::string dir = Directory();
    EnsureDir(dir);
    const std::string path = PathFor(name);

    std::ostringstream out;
    out << "# jewsploit config\n";
    auto& s = g_Settings;

    PutBool(out, "esp.enabled", s.esp.enabled);
    PutBool(out, "esp.draw_local", s.esp.draw_local);
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
    PutInt(out, "esp.mesh_chams_dx_mode_v3", s.esp.mesh_chams_dx_mode);
    PutBool(out, "esp.mesh_chams_occlusion", s.esp.mesh_chams_occlusion);
    PutInt(out, "esp.mesh_chams_occluded_dx_mode_v3", s.esp.mesh_chams_occluded_dx_mode);
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
    PutBool(out, "esp.apocalypse", s.esp.apocalypse);
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

    PutBool(out, "hitbox.enabled", s.hitbox.enabled);
    PutBool(out, "hitbox.visualize", s.hitbox.visualize);
    PutInt(out, "hitbox.viz_mode", s.hitbox.viz_mode);
    PutF4(out, "hitbox.viz_color", s.hitbox.viz_color);
    PutInt(out, "hitbox.part", s.hitbox.part);
    PutFloat(out, "hitbox.scale", s.hitbox.scale);

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
    PutBool(out, "misc.fly_gravity", s.misc.fly_gravity);
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

    PutInt(out, "gui.theme", s.gui.theme);
    PutInt(out, "gui.font", s.gui.font);
    PutBool(out, "gui.watermark", s.gui.watermark);
    PutFloat(out, "gui.watermark_x", s.gui.watermark_x);
    PutFloat(out, "gui.watermark_y", s.gui.watermark_y);
    for (int i = 0; i < Settings::WM_FIELD_COUNT; ++i) {
        char key[48];
        std::snprintf(key, sizeof(key), "gui.watermark_fields_%d", i);
        PutBool(out, key, s.gui.watermark_fields[i]);
    }
    PutF4(out, "gui.accent", s.gui.accent);
    PutF4(out, "gui.text_active", s.gui.text_active);
    PutF4(out, "gui.text_inactive", s.gui.text_inactive);
    PutF4(out, "gui.outer_border", s.gui.outer_border);
    PutF4(out, "gui.inner_border", s.gui.inner_border);
    PutF4(out, "gui.panel_fill", s.gui.panel_fill);
    PutF4(out, "gui.content_outer", s.gui.content_outer);
    PutF4(out, "gui.content_inner", s.gui.content_inner);
    PutF4(out, "gui.content_fill", s.gui.content_fill);
    PutF4(out, "gui.child_fill", s.gui.child_fill);

    PutBool(out, "killfx.enabled", s.killfx.enabled);
    PutInt(out, "killfx.effect", s.killfx.effect);
    PutBool(out, "hitmarker.enabled", s.hitmarker.enabled);
    PutFloat(out, "hitmarker.size", s.hitmarker.size);
    PutFloat(out, "hitmarker.duration", s.hitmarker.duration);
    PutBool(out, "hitsound.enabled", s.hitsound.enabled);
    PutInt(out, "hitsound.index", s.hitsound.index);
    PutFloat(out, "hitsound.volume", s.hitsound.volume);
    PutBool(out, "hitdata.enabled", s.hitdata.enabled);

    std::ofstream file(path, std::ios::trunc);
    if (!file)
        return false;
    file << out.str();
    return (bool)file;
}

// грузим key=value поверх того что есть
bool Load(const std::string& name)
{
    std::string path = PathFor(name);
    std::ifstream file(path);
    if (!file)
        return false;

    KV kv;
    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;

        auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;

        kv[line.substr(0, eq)] = line.substr(eq + 1);
    }

    auto& s = g_Settings;
    GetBool(kv, "esp.enabled", s.esp.enabled);
    GetBool(kv, "esp.draw_local", s.esp.draw_local);
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
    {
        const bool has_engine_key = GetBool(kv, "esp.engine_chams", s.esp.engine_chams);
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
    }
    GetInt(kv, "esp.chams_shader", s.esp.chams_shader);
    GetInt(kv, "esp.mesh_chams_style", s.esp.mesh_chams_style);
    LoadMeshDxMode(kv, s.esp.mesh_chams_dx_mode);
    GetBool(kv, "esp.mesh_chams_occlusion", s.esp.mesh_chams_occlusion);
    LoadMeshDxOccludedMode(kv, s.esp.mesh_chams_occluded_dx_mode);
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
    GetBool(kv, "esp.apocalypse", s.esp.apocalypse);
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
	if (kv.count("aim.silent_enabled"))
		GetBool(kv, "aim.silent_enabled", s.aim.silent_enabled);
	else if (s.aim.type == 2)
		s.aim.silent_enabled = true;
	GetInt(kv, "aim.silent_bind", s.aim.silent_bind);
	GetInt(kv, "aim.silent_bind_mode", s.aim.silent_bind_mode);

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
    {
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
    GetBool(kv, "misc.fly_gravity", s.misc.fly_gravity);
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

    GetInt(kv, "gui.theme", s.gui.theme);
    GetInt(kv, "gui.font", s.gui.font);
    GetBool(kv, "gui.watermark", s.gui.watermark);
    GetFloat(kv, "gui.watermark_x", s.gui.watermark_x);
    GetFloat(kv, "gui.watermark_y", s.gui.watermark_y);
    for (int i = 0; i < Settings::WM_FIELD_COUNT; ++i) {
        char key[48];
        std::snprintf(key, sizeof(key), "gui.watermark_fields_%d", i);
        GetBool(kv, key, s.gui.watermark_fields[i]);
    }
    GetF4(kv, "gui.accent", s.gui.accent);
    GetF4(kv, "gui.text_active", s.gui.text_active);
    GetF4(kv, "gui.text_inactive", s.gui.text_inactive);
    GetF4(kv, "gui.outer_border", s.gui.outer_border);
    GetF4(kv, "gui.inner_border", s.gui.inner_border);
    GetF4(kv, "gui.panel_fill", s.gui.panel_fill);
    GetF4(kv, "gui.content_outer", s.gui.content_outer);
    GetF4(kv, "gui.content_inner", s.gui.content_inner);
    GetF4(kv, "gui.content_fill", s.gui.content_fill);
    GetF4(kv, "gui.child_fill", s.gui.child_fill);

    GetBool(kv, "killfx.enabled", s.killfx.enabled);
    GetInt(kv, "killfx.effect", s.killfx.effect);
    GetBool(kv, "hitmarker.enabled", s.hitmarker.enabled);
    GetFloat(kv, "hitmarker.size", s.hitmarker.size);
    GetFloat(kv, "hitmarker.duration", s.hitmarker.duration);
    GetBool(kv, "hitsound.enabled", s.hitsound.enabled);
    GetInt(kv, "hitsound.index", s.hitsound.index);
    GetFloat(kv, "hitsound.volume", s.hitsound.volume);
    GetBool(kv, "hitdata.enabled", s.hitdata.enabled);

    return true;
}

bool Remove(const std::string& name)
{
    return DeleteFileA(PathFor(name).c_str()) != 0;
}

}
}

