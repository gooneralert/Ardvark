#include "pch.h"
#include "fonts.h"
#include "misc/imgui_freetype.h"
#include "font_fredoka_one.h"
#include "font_proxima_soft_bold.h"
#include "font_tahoma.h"
#include "font_tahoma_bold.h"
#include "font_proggyclean.h"
#include "font_visitor.h"

#include <Windows.h>
#include <string>
#include <cstring>

namespace fonts {

    ImFont* fredoka_one = nullptr;
    ImFont* proxima_soft_bold = nullptr;
    ImFont* imgui = nullptr;
    ImFont* tahoma_bold = nullptr;
    ImFont* proggy_clean = nullptr;
    ImFont* visitor = nullptr;
    ImFont* verdana = nullptr;
    ImFont* menu = nullptr;

    ImFont* tahoma = nullptr;
    ImFont* music_regular = nullptr;
    ImFont* music_bold = nullptr;
    ImFont* esp = nullptr;
    ImFont* esp_bold = nullptr;

    static void cfg_aa(ImFontConfig& c, float size, bool own_data)
    {
        c = ImFontConfig();
        c.PixelSnapH = false;
        c.OversampleH = 3;
        c.OversampleV = 2;
        c.RasterizerMultiply = 1.15f;
        c.FontDataOwnedByAtlas = own_data;
        c.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_LightHinting;
        c.SizePixels = size;
    }

    static ImFont* add_system(ImGuiIO& io, const char* file, float size, const ImWchar* ranges)
    {
        char path[MAX_PATH]{};
        if (GetWindowsDirectoryA(path, MAX_PATH) <= 0)
            return nullptr;
        strcat_s(path, "\\Fonts\\");
        strcat_s(path, file);

        ImFontConfig cfg;
        cfg_aa(cfg, size, true);
        return io.Fonts->AddFontFromFileTTF(path, size, &cfg, ranges);
    }

    static void merge_cyr(ImGuiIO& io, float size, const ImWchar* ranges_cyr)
    {
        ImFontConfig cfg;
        cfg_aa(cfg, size, false);
        cfg.MergeMode = true;
        io.Fonts->AddFontFromMemoryTTF(Tahoma, sizeof(Tahoma), size, &cfg, ranges_cyr);
    }

    void load(ImGuiIO& io)
    {
        io.Fonts->SetFontLoader(ImGuiFreeType::GetFontLoader());
        io.Fonts->FontLoaderFlags = ImGuiFreeTypeLoaderFlags_LightHinting;

        const ImWchar* ranges_def = io.Fonts->GetGlyphRangesDefault();
        const ImWchar* ranges_cyr = io.Fonts->GetGlyphRangesCyrillic();

        menu = io.Fonts->AddFontDefault();
        merge_cyr(io, 13.f, ranges_cyr);

        ImFontConfig fk;
        cfg_aa(fk, 15.f, false);
        fredoka_one = io.Fonts->AddFontFromMemoryTTF(
            FredokaOne, sizeof(FredokaOne), 15.f, &fk, ranges_def);
        merge_cyr(io, 15.f, ranges_cyr);

        ImFontConfig psb;
        cfg_aa(psb, 14.f, false);
        proxima_soft_bold = io.Fonts->AddFontFromMemoryTTF(
            ProximaSoftBold, sizeof(ProximaSoftBold), 14.f, &psb, ranges_def);
        merge_cyr(io, 14.f, ranges_cyr);

        ImFontConfig tah;
        cfg_aa(tah, 14.f, false);
        tahoma = io.Fonts->AddFontFromMemoryTTF(
            Tahoma, sizeof(Tahoma), 14.f, &tah, ranges_cyr);
        imgui = tahoma;
        if (!imgui)
        {
            imgui = add_system(io, "segoeui.ttf", 14.f, ranges_cyr);
            tahoma = imgui;
        }
        if (!imgui)
        {
            ImFontConfig def;
            cfg_aa(def, 14.f, true);
            imgui = io.Fonts->AddFontDefault(&def);
            merge_cyr(io, 14.f, ranges_cyr);
            tahoma = imgui;
        }

        ImFontConfig bold;
        cfg_aa(bold, 14.f, false);
        tahoma_bold = io.Fonts->AddFontFromMemoryTTF(
            TahomaBold, sizeof(TahomaBold), 14.f, &bold, ranges_def);
        merge_cyr(io, 14.f, ranges_cyr);

        ImFontConfig prog;
        cfg_aa(prog, 13.f, false);
        proggy_clean = io.Fonts->AddFontFromMemoryTTF(
            ProggyClean, sizeof(ProggyClean), 13.f, &prog, ranges_def);
        merge_cyr(io, 13.f, ranges_cyr);

        ImFontConfig vis;
        cfg_aa(vis, 12.f, false);
        visitor = io.Fonts->AddFontFromMemoryTTF(
            Visitor, sizeof(Visitor), 12.f, &vis, ranges_def);
        merge_cyr(io, 12.f, ranges_cyr);

        verdana = add_system(io, "verdana.ttf", 14.f, ranges_cyr);
        if (!verdana)
            verdana = tahoma;

        esp = fredoka_one ? fredoka_one : tahoma;
        esp_bold = tahoma_bold ? tahoma_bold : tahoma;

        // music player fonts: Inter (SIL OFL) from disk, falling back to tahoma
        {
            char base[MAX_PATH] = {};
            GetModuleFileNameA(nullptr, base, MAX_PATH);
            char* slash = strrchr(base, '\\');
            if (slash) *(slash + 1) = 0;
            const std::string dir = base;

            ImFontConfig mreg;
            cfg_aa(mreg, 18.f, true);
            music_regular = io.Fonts->AddFontFromFileTTF(
                (dir + "..\\..\\src\\music_player\\assets\\fonts\\Inter-Regular.ttf").c_str(),
                18.f, &mreg, ranges_def);
            if (!music_regular)
                music_regular = io.Fonts->AddFontFromFileTTF(
                    "src/music_player/assets/fonts/Inter-Regular.ttf", 18.f, &mreg, ranges_def);

            ImFontConfig mbold;
            cfg_aa(mbold, 18.f, true);
            music_bold = io.Fonts->AddFontFromFileTTF(
                (dir + "..\\..\\src\\music_player\\assets\\fonts\\Inter-SemiBold.ttf").c_str(),
                18.f, &mbold, ranges_def);
            if (!music_bold)
                music_bold = io.Fonts->AddFontFromFileTTF(
                    "src/music_player/assets/fonts/Inter-SemiBold.ttf", 18.f, &mbold, ranges_def);
        }
        if (!music_regular) music_regular = tahoma;
        if (!music_bold)    music_bold = tahoma_bold ? tahoma_bold : tahoma;

        io.FontDefault = menu ? menu : (imgui ? imgui : tahoma);
    }

}
