#pragma once

#include "ConfigIo.h"
#include "app/Settings.h"

namespace Cheat {
namespace Config {
namespace detail {

inline void WriteGui(std::ostringstream& out, const Settings& s)
{
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
}

inline void ReadGui(const KV& kv, Settings& s)
{
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
}

} // namespace detail
} // namespace Config
} // namespace Cheat
