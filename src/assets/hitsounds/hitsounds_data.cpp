#include "hitsounds_data.h"
#include "hitsounds_res.h"

#include <Windows.h>

namespace Cheat::Features::HitSoundsData {
namespace {

struct ResEntry {
    const char* name;
    int rid;
};

const ResEntry k_res[] = {
    { "12", 31000 },
    { "300-bucks", 31001 },
    { "4", 31002 },
    { "AAAAAA", 31003 },
    { "agpa1", 31004 },
    { "agpa2", 31005 },
    { "ahhhh-gachi-muchi", 31006 },
    { "aimbooster", 31007 },
    { "animezing", 31008 },
    { "applepay", 31009 },
    { "apply", 31010 },
    { "archive", 31011 },
    { "basshit", 31012 },
    { "beap", 31013 },
    { "beep", 31014 },
    { "beep1", 31015 },
    { "bell", 31016 },
    { "ben", 31017 },
    { "bepis", 31018 },
    { "bonk", 31019 },
    { "brotato", 31020 },
    { "bubble", 31021 },
    { "Buttonbell", 31022 },
    { "cash", 31023 },
    { "click", 31024 },
    { "cod", 31025 },
    { "coinmaster", 31026 },
    { "connectpacanoff", 31027 },
    { "copperbell", 31028 },
    { "cowbell", 31029 },
    { "crowbar", 31030 },
    { "DeadCellsHit", 31031 },
    { "dog", 31032 },
    { "door stop", 31033 },
    { "doorsmasher", 31034 },
    { "dsr-1", 31035 },
    { "duck", 31036 },
    { "dungeon-master", 31037 },
    { "f12", 31038 },
    { "fatality", 31039 },
    { "fire", 31040 },
    { "fortnite-dead", 31041 },
    { "Groteks", 31042 },
    { "gta pickup", 31043 },
    { "hentai", 31044 },
    { "huhh", 31045 },
    { "kick", 31046 },
    { "klatsch", 31047 },
    { "mariowoahh", 31048 },
    { "mcbow", 31049 },
    { "metal", 31050 },
    { "metal-pipe", 31051 },
    { "mgs", 31052 },
    { "minecraft bow", 31053 },
    { "minecraft button", 31054 },
    { "minecraft egg throw", 31055 },
    { "minecraft hit", 31056 },
    { "minecraft XP gain", 31057 },
    { "minecraft-critical-hit", 31058 },
    { "minecraft-explode1", 31059 },
    { "minecrafy old hitsound", 31060 },
    { "mmm-gachi", 31061 },
    { "money claim", 31062 },
    { "moneypickup1", 31063 },
    { "moneypickup2", 31064 },
    { "mouthsound", 31065 },
    { "msfrs", 31066 },
    { "my name jeff", 31067 },
    { "neverlose", 31068 },
    { "oh-shit-iam-sorry", 31069 },
    { "Oscock", 31070 },
    { "pam", 31071 },
    { "Pan", 31072 },
    { "plasmablaster", 31073 },
    { "poolplayer", 31074 },
    { "poordrum", 31075 },
    { "pop", 31076 },
    { "primordial", 31077 },
    { "Punch", 31078 },
    { "punchy", 31079 },
    { "quake", 31080 },
    { "quaver", 31081 },
    { "raiting", 31082 },
    { "regulus", 31083 },
    { "reminder", 31084 },
    { "roblox", 31085 },
    { "rust headshot", 31086 },
    { "satisfying click", 31087 },
    { "skeet", 31088 },
    { "sonic", 31089 },
    { "spiral knight", 31090 },
    { "stapler", 31091 },
    { "talk", 31092 },
    { "tavern misc1c", 31093 },
    { "time", 31094 },
    { "ting", 31095 },
    { "trident pierce", 31096 },
    { "uber", 31097 },
    { "unreal1", 31098 },
    { "Void", 31099 },
    { "warning", 31100 },
    { "water drop", 31101 },
    { "window xp error", 31102 },
    { "woo", 31103 },
    { "wood impact", 31104 },
    { "wood strain", 31105 },
    { "xpUp", 31106 },
    { "zelda", 31107 },
    { "zingtrio", 31108 },
};

constexpr int k_res_count = 109;

struct Loaded {
    const unsigned char* data;
    std::size_t size;
};

Loaded g_loaded[k_res_count]{};
bool g_ready = false;

void EnsureLoaded()
{
    if (g_ready) return;
    HMODULE mod = GetModuleHandleW(nullptr);
    for (int i = 0; i < k_res_count; ++i) {
        HRSRC hrsrc = FindResourceW(mod, MAKEINTRESOURCEW(k_res[i].rid), RT_RCDATA);
        if (!hrsrc) continue;
        HGLOBAL hglob = LoadResource(mod, hrsrc);
        if (!hglob) continue;
        void* ptr = LockResource(hglob);
        const DWORD sz = SizeofResource(mod, hrsrc);
        if (!ptr || sz == 0) continue;
        g_loaded[i].data = static_cast<const unsigned char*>(ptr);
        g_loaded[i].size = static_cast<std::size_t>(sz);
    }
    g_ready = true;
}

}

static Entry g_entries[k_res_count]{};
static bool g_entries_ready = false;

const Entry* Entries()
{
    EnsureLoaded();
    if (!g_entries_ready) {
        for (int i = 0; i < k_res_count; ++i) {
            g_entries[i].name = k_res[i].name;
            g_entries[i].data = g_loaded[i].data;
            g_entries[i].size = g_loaded[i].size;
        }
        g_entries_ready = true;
    }
    return g_entries;
}

int Count()
{
    return k_res_count;
}

const Entry& At(int index)
{
    const Entry* e = Entries();
    return e[index];
}

}
