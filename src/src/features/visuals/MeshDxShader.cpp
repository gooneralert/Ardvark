#include "pch.h"
#include "MeshDxShader.h"
#include "MeshCache.h"
#include "features/visuals/RaycastEngine.h"

#include "app/Settings.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"

#include <d3dcompiler.h>

#include <cstring>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "d3dcompiler.lib")

namespace Cheat {
namespace Visuals {
namespace MeshDxShader {
namespace {

constexpr char k_hlsl[] =
R"HLSL(
cbuffer Constants : register(b0)
{
    row_major float4x4 view;
    row_major float4x4 world;
    float3 camera;
    float  time;
    float4 base_color;
    float4 fresnel_color;
    float4 visible_color;
    float4 occluded_color;
    float4 occluded_fresnel;
    int    mode;
    float  fresnel_power;
    int    occlusion_enabled;
    int    occluded_mode;
    float4 outline_color;
    float  outline_fade;
    int    outline_style;
    int    outline_enabled;
    float  glow_strength;
};

Texture2D<float> world_depth : register(t0);
Texture2D<float> cham_depth  : register(t1);

struct VSIn
{
    float3 pos    : POSITION;
    float3 normal : NORMAL;
    float2 uv     : TEXCOORD0;
};

struct PSIn
{
    float4 pos    : SV_POSITION;
    float3 wpos   : TEXCOORD0;
    float3 normal : NORMAL;
    float2 uv     : TEXCOORD1;
    float3 lpos   : TEXCOORD2; // mesh local — аним не едет при ходьбе
};

PSIn vs_main(VSIn i)
{
    PSIn o;
    float4 wp = mul(world, float4(i.pos, 1.0));
    o.pos     = mul(view, wp);

    float dist = length(wp.xyz - camera);
    o.pos.z   = saturate(dist / 5000.0) * o.pos.w;

    o.wpos = wp.xyz;
    o.lpos = i.pos;
    // non-uniform scale ломает обычный mul — для chams хватает face-normal из экрана
    o.normal = mul((float3x3)world, i.normal);
    o.uv = i.uv;
    return o;
}

float4 ps_depth(PSIn i) : SV_TARGET
{
    return float4(0, 0, 0, 0);
}

float4 ps_main(PSIn i) : SV_TARGET
{
    float3 V = normalize(camera - i.wpos);
    // всегда face normal из производных — двусторонний, без дыр от clip/кривых normal после scale
    float3 dn = cross(ddy(i.wpos), ddx(i.wpos));
    float3 face_n = normalize(dn);
    if (dot(face_n, V) < 0.0)
        face_n = -face_n;
    float facing = saturate(dot(face_n, V));

    float3 N    = face_n;
    float  ndv  = facing;
    float  fres = pow(saturate(1.0 - ndv), max(0.1, fresnel_power));
    float3 L    = normalize(float3(0.45, 0.85, 0.30));
    float  ndl  = saturate(dot(N, L));
    float  lambert = ndl * 0.65 + 0.35;
    float3 H    = normalize(L + V);
    float  ndh  = saturate(dot(N, H));
    // fake env: hemisphere from normal
    float3 env  = pow(saturate(N * 0.5 + 0.5), 1.4);

    float4 bc = base_color;
    float4 fc = fresnel_color;
    int    m  = mode;

    if (occlusion_enabled != 0)
    {
        float cham_d  = i.pos.z;
        int2  sp      = int2(i.pos.xy);
        float w0 = world_depth.Load(int3(sp, 0));
        float w1 = world_depth.Load(int3(sp + int2(1, 0), 0));
        float w2 = world_depth.Load(int3(sp + int2(-1, 0), 0));
        float w3 = world_depth.Load(int3(sp + int2(0, 1), 0));
        float w4 = world_depth.Load(int3(sp + int2(0, -1), 0));
        float world_d = max(w0, max(w1, max(w2, max(w3, w4))));
        // dist-depth: больше = дальше; стена ближе → occluded
        if (cham_d > world_d + 0.0006)
        {
            bc = occluded_color;
            fc = occluded_fresnel;
            m  = occluded_mode;
        }
        else
        {
            bc = visible_color;
            fc = fresnel_color;
            m  = mode;
        }
    }

    float4 result = bc;

    if (m == 0) // flat
    {
        result.rgb = bc.rgb;
        result.a   = 1.0;
    }
    else if (m == 1) // chrome
    {
        float  spec   = pow(ndv, 14.0);
        result.rgb    = env * bc.rgb + spec.xxx * 0.6 + fc.rgb * fres;
        result.a      = 1.0;
    }
    else if (m == 2) // rainbow
    {
        float3 rainbow = 0.5 + 0.5 * cos(time * 2.0 + i.lpos * 0.35 + float3(0.0, 2.0, 4.0));
        result.rgb     = rainbow * lambert;
        result.a       = 1.0;
    }
    else if (m == 3) // pearl
    {
        float3 iri = 0.5 + 0.5 * cos(ndv * 6.2831 + float3(0.0, 2.1, 4.2));
        float spec = pow(ndh, 48.0) * 0.7;
        result.rgb = bc.rgb * lambert + iri * fres * 0.85 + fc.rgb * fres * 0.35 + spec.xxx;
        result.a   = 1.0;
    }
    else if (m == 4) // glossy
    {
        float wrap = saturate(ndl * 0.55 + 0.45);
        float spec = pow(ndh, 128.0) * 1.1;
        float rim  = pow(saturate(1.0 - ndv), 2.5) * 0.4;
        result.rgb = bc.rgb * wrap + spec.xxx + fc.rgb * rim + env * fc.rgb * 0.12;
        result.a   = 1.0;
    }
    else if (m == 5) // holographic
    {
        float3 holo = 0.5 + 0.5 * cos(time * 3.0 + i.lpos.y * 2.0 + ndv * 8.0 + float3(0.0, 2.0, 4.0));
        float spec = pow(ndh, 80.0) * 0.9;
        result.rgb = bc.rgb * lambert * 0.55 + holo * (fres * 0.9 + 0.15) + fc.rgb * fres * 0.25 + spec.xxx;
        result.a   = 1.0;
    }
    else if (m == 6) // fade — soft multi-color blend
    {
        float t = time * 0.45;
        // крупные мягкие волны по телу (без frac-тайлинга → нет швов)
        float3 p = i.lpos * 0.07;
        float n1 = sin(p.x * 1.3 + p.y * 0.9 + t);
        float n2 = sin(p.y * 1.1 - p.z * 1.0 + t * 0.85 + 2.1);
        float n3 = sin(p.z * 1.2 + p.x * 0.8 - t * 0.7 + 4.2);
        // domain warp — плавнее пятна
        float w = sin(n1 * 1.4 + n2) * 0.55;
        float u = saturate(0.5 + 0.5 * (n1 + w));
        float v = saturate(0.5 + 0.5 * (n2 - w * 0.6));
        float s = saturate(0.5 + 0.5 * (n3 + n1 * 0.35));

        float3 cA = float3(0.95, 0.35, 0.70); // pink
        float3 cB = float3(0.45, 0.55, 1.00); // blue
        float3 cC = float3(0.40, 0.95, 0.85); // mint
        float3 cD = float3(1.00, 0.70, 0.35); // peach
        float3 cE = float3(0.70, 0.40, 0.95); // lilac

        float3 col = lerp(cA, cB, u);
        col = lerp(col, cC, v * 0.85);
        col = lerp(col, cD, s * 0.70);
        col = lerp(col, cE, (1.0 - u) * v * 0.55);
        col += fres * 0.08;

        result.rgb = saturate(col);
        result.a   = 1.0;
    }
    else if (m == 7) // wireframe (FILL_WIREFRAME + solid color)
    {
        result.rgb = bc.rgb;
        result.a   = 1.0;
    }
    else if (m == 8) // glass
    {
        float f = saturate(fres * 1.45 + 0.08);
        float3 tint = lerp(bc.rgb * 0.42, float3(0.85, 0.92, 1.0), 0.35);
        result.rgb = lerp(tint, saturate(bc.rgb + 0.35), f) + pow(ndh, 72.0) * 0.45;
        result.a   = saturate(0.22 + f * 0.62) * max(bc.a, 0.35);
    }
    else if (m == 9) // ropes — copper diamond lattice, scrolls over body
    {
        float2 wp = float2(i.lpos.x + i.lpos.y, i.lpos.y + i.lpos.z) * 2.8
                  + float2(time * 1.35, time * 0.95);
        // fallback if uv broken — mix in mesh uv
        wp += i.uv * 6.0;
        float2 r = float2(wp.x + wp.y, wp.x - wp.y) * 0.7071;
        float2 cell = abs(frac(r) - 0.5);
        float d = min(cell.x, cell.y);
        float rope_w = 1.0 - smoothstep(0.0, 0.07, d);
        float core = 1.0 - smoothstep(0.0, 0.028, d);
        float3 copper = float3(1.00, 0.42, 0.10);
        float3 hi     = float3(1.00, 0.92, 0.55);
        float3 dark   = float3(0.06, 0.03, 0.02);
        float3 rope = lerp(copper, hi, core);
        result.rgb = lerp(dark, rope, rope_w) * (0.85 + fres * 0.35);
        result.a   = saturate(0.25 + rope_w * 0.85);
    }
    else if (m == 10) // liquid metal — animated mercury
    {
        float3 p = i.lpos * 0.9;
        float t = time;
        // бегущие волны по поверхности
        float wA = sin(p.x * 3.4 + p.y * 2.1 - t * 2.4);
        float wB = cos(p.y * 3.8 + p.z * 2.6 + t * 1.9);
        float wC = sin(dot(p, float3(2.2, 1.6, 2.8)) + t * 2.8);
        float flow = wA + wB * 0.85 + wC * 0.55;
        float ripple = 0.5 + 0.5 * sin(flow * 3.0 + ndv * 8.0 + t * 1.5);
        float streak = pow(saturate(0.5 + 0.5 * sin(p.y * 14.0 - t * 6.0 + flow * 2.0)), 6.0);

        float3 dark = float3(0.16, 0.18, 0.20);
        float3 mid  = float3(0.48, 0.50, 0.53);
        float3 hi   = float3(0.92, 0.94, 0.97);
        float3 iri  = 0.5 + 0.5 * cos(flow * 2.4 + t * 1.2 + ndv * 10.0 + float3(0.0, 2.1, 4.2));

        float3 metal = lerp(dark, mid, saturate(0.45 + lambert * 0.4 + flow * 0.08));
        metal = lerp(metal, hi, pow(ndh, 36.0) * 0.95 + pow(saturate(ndv), 8.0) * 0.4);
        metal += streak * hi * 0.55;
        metal += iri * fres * (0.18 + ripple * 0.28);
        // искажённый fake-env — «жидкость»
        float3 envW = pow(saturate(N * 0.5 + 0.5 + float3(flow, -flow, ripple) * 0.08), 1.2);
        metal += envW * 0.22 * (0.5 + ripple * 0.5);
        result.rgb = saturate(metal);
        result.a   = 1.0;
    })HLSL"
R"HLSL(
    else if (m == 11) // soft glass — тоньше glass, fill
    {
        float f = saturate(fres * 1.2 + 0.05);
        float3 tint = lerp(bc.rgb * 0.55, float3(0.78, 0.88, 1.0), 0.4);
        result.rgb = lerp(tint, saturate(bc.rgb + 0.25), f * 0.85) + pow(ndh, 90.0) * 0.35;
        result.a   = saturate(0.12 + f * 0.48) * max(bc.a, 0.25);
    }
    else if (m == 12) // ice — холодное стекло без аним
    {
        float f = saturate(fres * 1.6 + 0.1);
        float3 ice = lerp(float3(0.55, 0.75, 0.95), float3(0.9, 0.97, 1.0), f);
        ice = lerp(ice, bc.rgb, 0.35);
        result.rgb = ice + pow(ndh, 64.0) * 0.55;
        result.a   = saturate(0.2 + f * 0.55) * max(bc.a, 0.3);
    }
    else if (m == 13) // ghost pulse — alpha дышит
    {
        float pulse = 0.55 + 0.45 * sin(time * 2.4);
        float rim = pow(saturate(1.0 - ndv), 1.8);
        result.rgb = bc.rgb * (0.4 + rim * 0.6) + fc.rgb * fres * 0.3;
        result.a   = saturate((0.14 + rim * 0.5) * pulse) * max(bc.a, 0.28);
    }
    else if (m == 14) // aurora soft — полупроз радуга
    {
        float3 p = i.lpos * 0.12;
        float t = time * 0.7;
        float3 band = 0.5 + 0.5 * cos(t + p.y * 3.0 + p.x * 1.2 + float3(0.0, 2.1, 4.2));
        float veil = 0.35 + 0.45 * sin(p.y * 2.0 + t * 1.3);
        result.rgb = lerp(bc.rgb * 0.5, band, 0.65) * (0.55 + fres * 0.55);
        result.a   = saturate(0.16 + veil * 0.4 + fres * 0.25) * max(bc.a, 0.25);
    }
    else if (m == 15) // bubble — иридесцент пузыри
    {
        float2 q = i.uv * 8.0 + float2(time * 0.6, time * 0.35);
        float2 cell = frac(q) - 0.5;
        float d = length(cell);
        float bubble = 1.0 - smoothstep(0.18, 0.42, d);
        float ring = smoothstep(0.28, 0.34, d) * (1.0 - smoothstep(0.34, 0.42, d));
        float3 iri = 0.5 + 0.5 * cos(d * 18.0 + time * 2.0 + float3(0.0, 2.0, 4.0));
        result.rgb = lerp(bc.rgb * 0.45, iri, saturate(bubble * 0.7 + ring)) + fres * 0.2;
        result.a   = saturate(0.12 + bubble * 0.35 + ring * 0.55 + fres * 0.2) * max(bc.a, 0.25);
    }
    else if (m == 16) // jelly — дрожащий гель
    {
        float3 p = i.lpos * 1.4;
        float wob = sin(p.x * 4.0 + time * 3.2) * cos(p.y * 3.5 - time * 2.6);
        float blob = 0.5 + 0.5 * wob;
        float3 gel = lerp(bc.rgb * 0.55, bc.rgb + float3(0.15, 0.25, 0.2), blob);
        gel += pow(ndh, 40.0) * 0.4;
        result.rgb = saturate(gel);
        result.a   = saturate(0.22 + blob * 0.28 + fres * 0.35) * max(bc.a, 0.3);
    }
    else if (m == 17) // mercury soft — ртуть полупроз
    {
        float3 p = i.lpos * 1.1;
        float t = time;
        float wA = sin(p.x * 3.2 + p.y * 2.0 - t * 2.2);
        float wB = cos(p.y * 3.6 + p.z * 2.4 + t * 1.7);
        float flow = wA + wB * 0.8;
        float ripple = 0.5 + 0.5 * sin(flow * 2.8 + ndv * 7.0 + t * 1.4);
        float streak = pow(saturate(0.5 + 0.5 * sin(p.y * 12.0 - t * 5.5 + flow)), 5.5);
        float3 dark = float3(0.22, 0.24, 0.28);
        float3 mid  = float3(0.55, 0.58, 0.62);
        float3 hi   = float3(0.95, 0.97, 1.0);
        float3 iri  = 0.5 + 0.5 * cos(flow * 2.0 + t + ndv * 9.0 + float3(0.0, 2.1, 4.2));
        float3 merc = lerp(dark, mid, saturate(0.4 + lambert * 0.35 + flow * 0.07));
        merc = lerp(merc, hi, pow(ndh, 42.0) * 0.9 + streak * 0.5);
        merc = lerp(merc, bc.rgb, 0.28);
        merc += iri * fres * (0.22 + ripple * 0.25);
        result.rgb = saturate(merc);
        result.a   = saturate(0.2 + fres * 0.45 + ripple * 0.12) * max(bc.a, 0.3);
    }
    else if (m == 18) // water glass — вода + каустики
    {
        float3 p = i.lpos * 0.55;
        float t = time * 1.15;
        float c1 = sin(p.x * 4.0 + p.z * 3.0 + t * 2.0);
        float c2 = cos(p.y * 3.5 - p.x * 2.5 + t * 1.6);
        float cau = saturate(0.55 + 0.45 * (c1 * c2));
        float spark = pow(saturate(cau), 8.0);
        float3 water = lerp(float3(0.15, 0.45, 0.75), float3(0.55, 0.85, 1.0), cau);
        water = lerp(water, bc.rgb, 0.4);
        water += spark * float3(0.85, 0.95, 1.0) * 0.55;
        water += pow(ndh, 70.0) * 0.4;
        result.rgb = saturate(water);
        result.a   = saturate(0.16 + fres * 0.5 + cau * 0.15) * max(bc.a, 0.28);
    }
    else if (m == 19) // deep ocean — тёмная вода
    {
        float3 p = i.lpos * 0.2;
        float t = time * 0.55;
        float wave = sin(p.x * 2.0 + t) * cos(p.z * 1.7 - t * 0.8);
        float depth = saturate(0.35 + wave * 0.2 + fres * 0.45);
        float3 deep = float3(0.02, 0.12, 0.28);
        float3 shallow = float3(0.12, 0.45, 0.7);
        float3 col = lerp(deep, shallow, depth);
        col = lerp(col, bc.rgb * 0.7, 0.35);
        col += pow(ndh, 90.0) * 0.35;
        result.rgb = saturate(col);
        result.a   = saturate(0.22 + depth * 0.35 + fres * 0.25) * max(bc.a, 0.32);
    }
    else if (m == 20) // quicksilver — блестящая ртуть
    {
        float3 p = i.lpos * 1.6;
        float t = time * 1.8;
        float drip = sin(p.y * 6.0 - t * 4.0 + sin(p.x * 3.0 + t) * 1.5);
        float blob = saturate(0.5 + 0.5 * drip);
        float bead = pow(saturate(abs(drip)), 3.0);
        float3 silv = lerp(float3(0.35, 0.38, 0.42), float3(0.92, 0.94, 0.98), blob);
        silv = lerp(silv, bc.rgb, 0.25);
        silv += bead * 0.65 + pow(ndh, 28.0) * 0.85;
        silv += fres * float3(0.7, 0.85, 1.0) * 0.35;
        result.rgb = saturate(silv);
        result.a   = saturate(0.18 + blob * 0.35 + fres * 0.4) * max(bc.a, 0.28);
    }
    else if (m == 21) // ripple — кольца по воде
    {
        float2 q = float2(i.lpos.x, i.lpos.z) * 0.9;
        float t = time * 2.2;
        float d0 = length(q + float2(sin(t * 0.3), cos(t * 0.25)) * 0.4);
        float d1 = length(q - float2(0.7, -0.3));
        float rings = sin(d0 * 14.0 - t * 5.0) * 0.5 + sin(d1 * 11.0 - t * 3.8) * 0.35;
        rings = saturate(0.5 + rings * 0.5);
        float crest = pow(rings, 4.0);
        float3 col = lerp(bc.rgb * 0.45, float3(0.4, 0.75, 1.0), rings);
        col += crest * 0.55 + fres * 0.25;
        result.rgb = saturate(col);
        result.a   = saturate(0.14 + rings * 0.35 + fres * 0.4) * max(bc.a, 0.26);
    }
    else if (m == 22) // oil slick — плёнка на воде
    {
        float3 p = i.lpos * 0.85;
        float t = time * 0.9;
        float flow = sin(p.x * 2.5 + t) + cos(p.y * 3.0 - t * 1.2);
        float3 film = 0.5 + 0.5 * cos(flow * 3.5 + ndv * 12.0 + t * 1.5 + float3(0.0, 2.2, 4.4));
        float wet = saturate(0.4 + fres * 0.7);
        float3 col = lerp(bc.rgb * 0.4, film, 0.75);
        col += pow(ndh, 55.0) * 0.45;
        result.rgb = saturate(col);
        result.a   = saturate(0.15 + wet * 0.45 + abs(flow) * 0.05) * max(bc.a, 0.28);
    }

    return result;
}

// fullscreen outline: edge-detect по depth mesh chams (+ wallcheck как occluded)
struct FSIn
{
    float4 pos : SV_POSITION;
};

FSIn vs_fs(uint id : SV_VertexID)
{
    FSIn o;
    float2 uv = float2((id << 1) & 2, id & 2);
    o.pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return o;
}

// силуэт по cham depth; occluded НЕ режем — glow как на рефах идёт сквозь стены
bool OutlineSolid(int2 sp)
{
    uint tw, th;
    cham_depth.GetDimensions(tw, th);
    if (sp.x < 0 || sp.y < 0 || sp.x >= (int)tw || sp.y >= (int)th)
        return false;
    float d = cham_depth.Load(int3(sp, 0));
    return d <= 0.9995;
}

float4 ps_outline(FSIn i) : SV_TARGET
{
    if (outline_enabled == 0)
        return float4(0, 0, 0, 0);

    int2 sp = int2(i.pos.xy);
    if (OutlineSolid(sp))
        return float4(0, 0, 0, 0);

    // было 24 луча × radius 24 ≈ 576 Load/пиксель на весь экран → FPS влетел
    // 8 лучей × radius 2..8, плюс coarse reject
    int radius = (int)(2.0 + saturate(outline_fade / 3.0) * 6.0);
    if (radius < 2) radius = 2;
    if (radius > 8) radius = 8;

    // быстрый отсев пустоты: только кольца r=1, mid, max (24 Load max)
    bool near = false;
    [unroll] for (int k = 0; k < 8; ++k)
    {
        float ang = (float)k * 0.78539816; // 2pi/8
        float2 dir = float2(cos(ang), sin(ang));
        int2 n1 = sp + int2(round(dir.x), round(dir.y));
        if (OutlineSolid(n1)) { near = true; break; }
        int mid = max(radius / 2, 2);
        int2 n2 = sp + int2(round(dir.x * (float)mid), round(dir.y * (float)mid));
        if (OutlineSolid(n2)) { near = true; break; }
        int2 n3 = sp + int2(round(dir.x * (float)radius), round(dir.y * (float)radius));
        if (OutlineSolid(n3)) { near = true; break; }
    }
    if (!near)
        return float4(0, 0, 0, 0);

    float nearest = 1e5;
    [unroll] for (int k = 0; k < 8; ++k)
    {
        float ang = (float)k * 0.78539816;
        float2 dir = float2(cos(ang), sin(ang));
        // шаг 2 — вдвое меньше Load, на soft halo незаметно
        [loop] for (int r = 1; r <= radius; r += 1)
        {
            int2 np = sp + int2(round(dir.x * (float)r), round(dir.y * (float)r));
            if (!OutlineSolid(np))
                continue;
            float dist = length(float2(np - sp));
            if (dist < nearest)
                nearest = dist;
            break;
        }
    }

    if (nearest > (float)radius + 0.5)
        return float4(0, 0, 0, 0);

    float t = saturate(nearest / (float)radius);
    float core = exp(-nearest * nearest * 0.35);
    float mid  = exp(-t * t * 2.2);
    float tail = pow(saturate(1.0 - t), 2.8);
    float fall = core * 0.55 + mid * 0.40 + tail * 0.35;

    float3 rgb = outline_color.rgb;
    float a = outline_color.a * fall * 0.90;
    float anim = 1.0;

    if (outline_style == 0) // soft fade
    {
        float breath = 0.85 + 0.15 * sin(time * 1.55);
        anim = breath;
        rgb *= 0.92 + 0.14 * core;
    }
    else if (outline_style == 1) // pulse
    {
        float wave = 0.5 + 0.5 * sin(nearest * 0.55 - time * 4.0);
        float pulse = 0.55 + 0.45 * (0.5 + 0.5 * sin(time * 2.6));
        anim = lerp(0.55, 1.15, wave) * pulse;
        fall = core * 0.50 + mid * (0.35 + 0.30 * wave) + tail * 0.40;
        a = outline_color.a * fall * 0.92;
        rgb *= 0.85 + 0.35 * wave;
    }
    else if (outline_style == 2) // flow
    {
        float2 c = float2(sp) + 0.5;
        float band = sin(c.x * 0.065 + c.y * 0.048 - time * 3.2);
        float flow = 0.55 + 0.45 * (0.5 + 0.5 * band);
        anim = flow;
        rgb = lerp(rgb, saturate(rgb * 1.35 + 0.10), saturate(band * 0.40 + 0.25));
        a = outline_color.a * fall * (0.55 + 0.50 * flow);
    }
    else // neon
    {
        float ang = atan2((float)sp.y, (float)sp.x);
        float swirl = 0.5 + 0.5 * sin(ang * 3.0 + time * 2.5 + nearest * 0.25);
        float rim = exp(-nearest * nearest * 0.18);
        anim = 0.60 + 0.40 * swirl;
        fall = rim * 0.75 + mid * 0.35 + tail * 0.40;
        a = outline_color.a * fall * (0.65 + 0.45 * swirl);
        rgb = lerp(rgb, saturate(rgb + float3(0.20, 0.30, 0.50) * swirl), 0.35);
        rgb *= 0.90 + 0.40 * rim;
    }

    a *= anim;
    a *= saturate(1.10 - t * 0.75);
    if (a < 0.008)
        return float4(0, 0, 0, 0);
    return float4(rgb, saturate(a));
}
)HLSL";

struct CBData
{
	float view[16];
	float world[16];
	float camera[3];
	float time;
	float base_color[4];
	float fresnel_color[4];
	float visible_color[4];
	float occluded_color[4];
	float occluded_fresnel[4];
	int   mode;
	float fresnel_power;
	int   occlusion_enabled;
	int   occluded_mode;
	float outline_color[4];
	float outline_fade;
	int   outline_style;
	int   outline_enabled;
	float glow_strength;
};
static_assert(sizeof(CBData) % 16 == 0, "CBData");

struct GpuMesh
{
	ID3D11Buffer* vb = nullptr;
	ID3D11Buffer* ib = nullptr;
	UINT          index_count = 0;
};

struct DrawItem
{
	const GpuMesh* mesh = nullptr;
	Matrix4x4      world{};
};

ID3D11Device*            g_device = nullptr;
ID3D11DeviceContext*     g_context = nullptr;
ID3D11VertexShader*      g_vs = nullptr;
ID3D11VertexShader*      g_vs_fs = nullptr;
ID3D11PixelShader*       g_ps = nullptr;
ID3D11PixelShader*       g_ps_depth = nullptr;
ID3D11PixelShader*       g_ps_outline = nullptr;
ID3D11InputLayout*       g_layout = nullptr;
ID3D11Buffer*            g_cb = nullptr;
ID3D11BlendState*        g_blend = nullptr;
ID3D11BlendState*        g_blend_no_color = nullptr;
ID3D11DepthStencilState* g_ds = nullptr;
ID3D11DepthStencilState* g_ds_off = nullptr;
ID3D11RasterizerState*   g_raster = nullptr;
ID3D11RasterizerState*   g_raster_wire = nullptr;
ID3D11Texture2D*         g_depth_tex = nullptr;
ID3D11DepthStencilView*  g_dsv = nullptr;
ID3D11ShaderResourceView* g_cham_srv = nullptr;
ID3D11Texture2D*         g_world_depth_tex = nullptr;
ID3D11DepthStencilView*  g_world_dsv = nullptr;
ID3D11ShaderResourceView* g_world_srv = nullptr;

GpuMesh g_unit_cube{};
std::unordered_map<std::string, GpuMesh> g_uploaded;
std::vector<DrawItem> g_queue;
std::vector<Matrix4x4> g_world_boxes;

CBData   g_cbdata{};
unsigned g_width = 0;
unsigned g_height = 0;
bool     g_frame_valid = false;
Vector3  g_camera{};

const char* k_mode_names[] = {
	"flat", "chrome", "rainbow", "pearl", "glossy", "holographic",
	"fade", "wireframe", "glass", "ropes", "liquid metal",
	"soft glass", "ice", "ghost pulse", "aurora soft", "bubble", "jelly",
	"mercury soft", "water glass", "deep ocean", "quicksilver", "ripple", "oil slick"
};

bool ModeUsesFillColor(int m)
{
	// flat / wire / glass + полупроз с tint
	if (m == 0 || m == 7 || m == 8)
	{
		return true;
	}

	if (m >= 11 && m <= 22)
	{
		return true;
	}

	return false;
}

void ReleaseMesh(GpuMesh& mesh)
{
	if (mesh.vb) { mesh.vb->Release(); mesh.vb = nullptr; }
	if (mesh.ib) { mesh.ib->Release(); mesh.ib = nullptr; }
	mesh.index_count = 0;
}

GpuMesh Upload(const MeshVertex* verts, std::size_t vcount,
               const std::uint32_t* indices, std::size_t icount)
{
	GpuMesh out{};
	if (!g_device || !verts || !indices || vcount == 0 || icount == 0)
		return out;

	D3D11_BUFFER_DESC vbd{};
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = (UINT)(vcount * sizeof(MeshVertex));
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA vinit{ verts, 0, 0 };
	if (FAILED(g_device->CreateBuffer(&vbd, &vinit, &out.vb)))
		return out;

	D3D11_BUFFER_DESC ibd{};
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = (UINT)(icount * sizeof(std::uint32_t));
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA iinit{ indices, 0, 0 };
	if (FAILED(g_device->CreateBuffer(&ibd, &iinit, &out.ib)))
	{
		out.vb->Release();
		out.vb = nullptr;
		return out;
	}
	out.index_count = (UINT)icount;
	return out;
}

bool CreateDepth(unsigned w, unsigned h)
{
	if (g_world_srv) { g_world_srv->Release(); g_world_srv = nullptr; }
	if (g_world_dsv) { g_world_dsv->Release(); g_world_dsv = nullptr; }
	if (g_world_depth_tex) { g_world_depth_tex->Release(); g_world_depth_tex = nullptr; }
	if (g_cham_srv) { g_cham_srv->Release(); g_cham_srv = nullptr; }
	if (g_dsv) { g_dsv->Release(); g_dsv = nullptr; }
	if (g_depth_tex) { g_depth_tex->Release(); g_depth_tex = nullptr; }

	auto make_depth_srv = [&](ID3D11Texture2D** tex, ID3D11DepthStencilView** dsv,
	                          ID3D11ShaderResourceView** srv) -> bool {
		D3D11_TEXTURE2D_DESC td{};
		td.Width = w;
		td.Height = h;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_R32_TYPELESS;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		if (FAILED(g_device->CreateTexture2D(&td, nullptr, tex)))
			return false;

		D3D11_DEPTH_STENCIL_VIEW_DESC dsvd{};
		dsvd.Format = DXGI_FORMAT_D32_FLOAT;
		dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		if (FAILED(g_device->CreateDepthStencilView(*tex, &dsvd, dsv)))
			return false;

		D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
		srvd.Format = DXGI_FORMAT_R32_FLOAT;
		srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvd.Texture2D.MipLevels = 1;
		if (FAILED(g_device->CreateShaderResourceView(*tex, &srvd, srv)))
			return false;
		return true;
	};

	if (!make_depth_srv(&g_depth_tex, &g_dsv, &g_cham_srv))
		return false;
	if (!make_depth_srv(&g_world_depth_tex, &g_world_dsv, &g_world_srv))
		return false;

	g_width = w;
	g_height = h;
	return true;
}

Matrix4x4 MakeBoxWorld(const Vector3& pos, const Matrix4x4& rot, const Vector3& sz)
{
	Matrix4x4 w{};
	w.m[0][0] = rot.m[0][0] * sz.x;
	w.m[0][1] = rot.m[0][1] * sz.y;
	w.m[0][2] = rot.m[0][2] * sz.z;
	w.m[0][3] = pos.x;
	w.m[1][0] = rot.m[1][0] * sz.x;
	w.m[1][1] = rot.m[1][1] * sz.y;
	w.m[1][2] = rot.m[1][2] * sz.z;
	w.m[1][3] = pos.y;
	w.m[2][0] = rot.m[2][0] * sz.x;
	w.m[2][1] = rot.m[2][1] * sz.y;
	w.m[2][2] = rot.m[2][2] * sz.z;
	w.m[2][3] = pos.z;
	w.m[3][0] = 0.f;
	w.m[3][1] = 0.f;
	w.m[3][2] = 0.f;
	w.m[3][3] = 1.f;
	return w;
}

void EnsureDepthFromRtv(ID3D11RenderTargetView* rtv)
{
	if (g_dsv && g_world_dsv)
		return;
	unsigned w = g_width, h = g_height;
	if ((!w || !h) && rtv)
	{
		ID3D11Resource* res = nullptr;
		rtv->GetResource(&res);
		if (res)
		{
			ID3D11Texture2D* tex = nullptr;
			if (SUCCEEDED(res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&tex)) && tex)
			{
				D3D11_TEXTURE2D_DESC td{};
				tex->GetDesc(&td);
				w = td.Width;
				h = td.Height;
				tex->Release();
			}
			res->Release();
		}
	}
	if (w && h)
		CreateDepth(w, h);
}

bool CreateUnitCube()
{
	const MeshVertex verts[24] = {
		{ { 0.5f, -0.5f, -0.5f }, { 1, 0, 0 }, { 0, 0 }, 0, 0 },
		{ { 0.5f,  0.5f, -0.5f }, { 1, 0, 0 }, { 0, 0 }, 0, 0 },
		{ { 0.5f,  0.5f,  0.5f }, { 1, 0, 0 }, { 0, 0 }, 0, 0 },
		{ { 0.5f, -0.5f,  0.5f }, { 1, 0, 0 }, { 0, 0 }, 0, 0 },
		{ { -0.5f, -0.5f, -0.5f }, { -1, 0, 0 }, { 0, 0 }, 0, 0 },
		{ { -0.5f, -0.5f,  0.5f }, { -1, 0, 0 }, { 0, 0 }, 0, 0 },
		{ { -0.5f,  0.5f,  0.5f }, { -1, 0, 0 }, { 0, 0 }, 0, 0 },
		{ { -0.5f,  0.5f, -0.5f }, { -1, 0, 0 }, { 0, 0 }, 0, 0 },
		{ { -0.5f, 0.5f, -0.5f }, { 0, 1, 0 }, { 0, 0 }, 0, 0 },
		{ {  0.5f, 0.5f, -0.5f }, { 0, 1, 0 }, { 0, 0 }, 0, 0 },
		{ {  0.5f, 0.5f,  0.5f }, { 0, 1, 0 }, { 0, 0 }, 0, 0 },
		{ { -0.5f, 0.5f,  0.5f }, { 0, 1, 0 }, { 0, 0 }, 0, 0 },
		{ { -0.5f, -0.5f,  0.5f }, { 0, -1, 0 }, { 0, 0 }, 0, 0 },
		{ {  0.5f, -0.5f,  0.5f }, { 0, -1, 0 }, { 0, 0 }, 0, 0 },
		{ {  0.5f, -0.5f, -0.5f }, { 0, -1, 0 }, { 0, 0 }, 0, 0 },
		{ { -0.5f, -0.5f, -0.5f }, { 0, -1, 0 }, { 0, 0 }, 0, 0 },
		{ {  0.5f, -0.5f, 0.5f }, { 0, 0, 1 }, { 0, 0 }, 0, 0 },
		{ { -0.5f, -0.5f, 0.5f }, { 0, 0, 1 }, { 0, 0 }, 0, 0 },
		{ { -0.5f,  0.5f, 0.5f }, { 0, 0, 1 }, { 0, 0 }, 0, 0 },
		{ {  0.5f,  0.5f, 0.5f }, { 0, 0, 1 }, { 0, 0 }, 0, 0 },
		{ { -0.5f, -0.5f, -0.5f }, { 0, 0, -1 }, { 0, 0 }, 0, 0 },
		{ { -0.5f,  0.5f, -0.5f }, { 0, 0, -1 }, { 0, 0 }, 0, 0 },
		{ {  0.5f,  0.5f, -0.5f }, { 0, 0, -1 }, { 0, 0 }, 0, 0 },
		{ {  0.5f, -0.5f, -0.5f }, { 0, 0, -1 }, { 0, 0 }, 0, 0 },
	};
	const std::uint32_t idx[36] = {
		0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7,
		8, 9, 10, 8, 10, 11, 12, 13, 14, 12, 14, 15,
		16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
	};
	g_unit_cube = Upload(verts, 24, idx, 36);
	return g_unit_cube.vb != nullptr;
}

const GpuMesh* Fetch(const std::string& mesh_id)
{
	if (mesh_id.empty())
		return nullptr;

	if (auto it = g_uploaded.find(mesh_id); it != g_uploaded.end())
		return it->second.vb ? &it->second : nullptr;

	const auto mesh = MeshCache::Get().FindShared(mesh_id);
	if (!mesh || mesh->vertices.empty() || mesh->faces.empty())
		return nullptr;

	const std::size_t vcount = mesh->vertices.size();
	if (vcount > 50000)
		return nullptr;

	std::size_t fac_count = mesh->faces.size();
	if (fac_count > 20000)
		fac_count = 20000;

	std::vector<std::uint32_t> indices;
	indices.reserve(fac_count * 3);
	for (std::size_t i = 0; i < fac_count; ++i)
	{
		const auto& f = mesh->faces[i];
		if (f.indices[0] >= vcount || f.indices[1] >= vcount || f.indices[2] >= vcount)
			continue;
		indices.push_back(f.indices[0]);
		indices.push_back(f.indices[1]);
		indices.push_back(f.indices[2]);
	}
	if (indices.empty())
		return nullptr;

	if (g_uploaded.size() > 256)
	{
		for (auto& [_, m] : g_uploaded)
			ReleaseMesh(m);
		g_uploaded.clear();
	}

	GpuMesh gpu = Upload(mesh->vertices.data(), vcount, indices.data(), indices.size());
	auto [it, _] = g_uploaded.emplace(mesh_id, gpu);
	return it->second.vb ? &it->second : nullptr;
}

void Issue(const GpuMesh& mesh, const Matrix4x4& world)
{
	std::memcpy(g_cbdata.world, &world, sizeof(world));

	D3D11_MAPPED_SUBRESOURCE ms{};
	if (FAILED(g_context->Map(g_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms)))
		return;
	std::memcpy(ms.pData, &g_cbdata, sizeof(g_cbdata));
	g_context->Unmap(g_cb, 0);

	const UINT stride = sizeof(MeshVertex);
	const UINT offset = 0;
	g_context->IASetVertexBuffers(0, 1, &mesh.vb, &stride, &offset);
	g_context->IASetIndexBuffer(mesh.ib, DXGI_FORMAT_R32_UINT, 0);
	g_context->DrawIndexed(mesh.index_count, 0, 0);
}

} // namespace

bool Init(ID3D11Device* device, ID3D11DeviceContext* context)
{
	if (!device || !context)
		return false;

	g_device = device;
	g_context = context;

	ID3DBlob* vs_blob = nullptr;
	ID3DBlob* ps_blob = nullptr;
	ID3DBlob* err = nullptr;

	if (FAILED(D3DCompile(k_hlsl, sizeof(k_hlsl) - 1, "mesh_dx", nullptr, nullptr,
	                      "vs_main", "vs_4_0", 0, 0, &vs_blob, &err)))
	{
		if (err) err->Release();
		return false;
	}
	if (FAILED(D3DCompile(k_hlsl, sizeof(k_hlsl) - 1, "mesh_dx", nullptr, nullptr,
	                      "ps_main", "ps_4_0", 0, 0, &ps_blob, &err)))
	{
		if (err) err->Release();
		vs_blob->Release();
		return false;
	}

	if (FAILED(g_device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(),
	                                       nullptr, &g_vs)) ||
	    FAILED(g_device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(),
	                                      nullptr, &g_ps)))
	{
		vs_blob->Release();
		ps_blob->Release();
		return false;
	}

	ID3DBlob* ps_depth_blob = nullptr;
	if (FAILED(D3DCompile(k_hlsl, sizeof(k_hlsl) - 1, "mesh_dx", nullptr, nullptr,
	                      "ps_depth", "ps_4_0", 0, 0, &ps_depth_blob, &err)))
	{
		if (err) err->Release();
		vs_blob->Release();
		ps_blob->Release();
		return false;
	}
	if (FAILED(g_device->CreatePixelShader(ps_depth_blob->GetBufferPointer(),
	                                      ps_depth_blob->GetBufferSize(), nullptr, &g_ps_depth)))
	{
		ps_depth_blob->Release();
		vs_blob->Release();
		ps_blob->Release();
		return false;
	}
	ps_depth_blob->Release();

	ID3DBlob* vs_fs_blob = nullptr;
	ID3DBlob* ps_outline_blob = nullptr;
	if (FAILED(D3DCompile(k_hlsl, sizeof(k_hlsl) - 1, "mesh_dx", nullptr, nullptr,
	                      "vs_fs", "vs_4_0", 0, 0, &vs_fs_blob, &err)))
	{
		if (err) err->Release();
		vs_blob->Release();
		ps_blob->Release();
		return false;
	}
	if (FAILED(D3DCompile(k_hlsl, sizeof(k_hlsl) - 1, "mesh_dx", nullptr, nullptr,
	                      "ps_outline", "ps_4_0", 0, 0, &ps_outline_blob, &err)))
	{
		if (err) err->Release();
		vs_fs_blob->Release();
		vs_blob->Release();
		ps_blob->Release();
		return false;
	}
	if (FAILED(g_device->CreateVertexShader(vs_fs_blob->GetBufferPointer(),
	                                       vs_fs_blob->GetBufferSize(), nullptr, &g_vs_fs)) ||
	    FAILED(g_device->CreatePixelShader(ps_outline_blob->GetBufferPointer(),
	                                      ps_outline_blob->GetBufferSize(), nullptr, &g_ps_outline)))
	{
		vs_fs_blob->Release();
		ps_outline_blob->Release();
		vs_blob->Release();
		ps_blob->Release();
		return false;
	}
	vs_fs_blob->Release();
	ps_outline_blob->Release();

	const D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	if (FAILED(g_device->CreateInputLayout(layout, _countof(layout),
	                                      vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(),
	                                      &g_layout)))
	{
		vs_blob->Release();
		ps_blob->Release();
		return false;
	}
	vs_blob->Release();
	ps_blob->Release();

	D3D11_BUFFER_DESC cbd{};
	cbd.Usage = D3D11_USAGE_DYNAMIC;
	cbd.ByteWidth = sizeof(CBData);
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	if (FAILED(g_device->CreateBuffer(&cbd, nullptr, &g_cb)))
		return false;

	D3D11_BLEND_DESC bd{};
	bd.RenderTarget[0].BlendEnable = TRUE;
	bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	if (FAILED(g_device->CreateBlendState(&bd, &g_blend)))
		return false;

	D3D11_BLEND_DESC bd_no{};
	bd_no.RenderTarget[0].BlendEnable = FALSE;
	bd_no.RenderTarget[0].RenderTargetWriteMask = 0;
	if (FAILED(g_device->CreateBlendState(&bd_no, &g_blend_no_color)))
		return false;

	D3D11_DEPTH_STENCIL_DESC dsd{};
	dsd.DepthEnable = TRUE;
	dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	if (FAILED(g_device->CreateDepthStencilState(&dsd, &g_ds)))
		return false;

	D3D11_DEPTH_STENCIL_DESC dsd_off{};
	dsd_off.DepthEnable = FALSE;
	dsd_off.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	dsd_off.DepthFunc = D3D11_COMPARISON_ALWAYS;
	if (FAILED(g_device->CreateDepthStencilState(&dsd_off, &g_ds_off)))
		return false;

	D3D11_RASTERIZER_DESC rd{};
	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_NONE;
	rd.DepthClipEnable = TRUE;
	if (FAILED(g_device->CreateRasterizerState(&rd, &g_raster)))
		return false;

	D3D11_RASTERIZER_DESC rdw = rd;
	rdw.FillMode = D3D11_FILL_WIREFRAME;
	rdw.AntialiasedLineEnable = TRUE;
	if (FAILED(g_device->CreateRasterizerState(&rdw, &g_raster_wire)))
		return false;

	return CreateUnitCube();
}

void Shutdown()
{
	g_queue.clear();
	g_world_boxes.clear();
	g_frame_valid = false;
	for (auto& [_, mesh] : g_uploaded)
		ReleaseMesh(mesh);
	g_uploaded.clear();
	ReleaseMesh(g_unit_cube);

	if (g_world_srv) { g_world_srv->Release(); g_world_srv = nullptr; }
	if (g_world_dsv) { g_world_dsv->Release(); g_world_dsv = nullptr; }
	if (g_world_depth_tex) { g_world_depth_tex->Release(); g_world_depth_tex = nullptr; }
	if (g_cham_srv) { g_cham_srv->Release(); g_cham_srv = nullptr; }
	if (g_dsv) { g_dsv->Release(); g_dsv = nullptr; }
	if (g_depth_tex) { g_depth_tex->Release(); g_depth_tex = nullptr; }
	if (g_raster_wire) { g_raster_wire->Release(); g_raster_wire = nullptr; }
	if (g_raster) { g_raster->Release(); g_raster = nullptr; }
	if (g_ds_off) { g_ds_off->Release(); g_ds_off = nullptr; }
	if (g_ds) { g_ds->Release(); g_ds = nullptr; }
	if (g_blend_no_color) { g_blend_no_color->Release(); g_blend_no_color = nullptr; }
	if (g_blend) { g_blend->Release(); g_blend = nullptr; }
	if (g_cb) { g_cb->Release(); g_cb = nullptr; }
	if (g_layout) { g_layout->Release(); g_layout = nullptr; }
	if (g_ps_outline) { g_ps_outline->Release(); g_ps_outline = nullptr; }
	if (g_ps_depth) { g_ps_depth->Release(); g_ps_depth = nullptr; }
	if (g_ps) { g_ps->Release(); g_ps = nullptr; }
	if (g_vs_fs) { g_vs_fs->Release(); g_vs_fs = nullptr; }
	if (g_vs) { g_vs->Release(); g_vs = nullptr; }
	g_device = nullptr;
	g_context = nullptr;
	g_width = g_height = 0;
}

void Resize(unsigned width, unsigned height)
{
	if (!g_device || width == 0 || height == 0)
		return;
	if (width == g_width && height == g_height && g_dsv)
		return;
	CreateDepth(width, height);
}

void BeginFrame(const Matrix4x4& view, const Vector3& camera, float time)
{
	g_queue.clear();
	g_world_boxes.clear();
	g_camera = camera;
	g_frame_valid = (g_vs && g_ps && g_ps_depth && g_vs_fs && g_ps_outline && g_cb);

	std::memcpy(g_cbdata.view, &view, sizeof(view));
	g_cbdata.camera[0] = camera.x;
	g_cbdata.camera[1] = camera.y;
	g_cbdata.camera[2] = camera.z;
	g_cbdata.time = time;

	const auto& st = g_Settings.esp;
	g_cbdata.mode = (st.mesh_chams_style == 1) ? st.mesh_chams_dx_mode : 0;
	if (g_cbdata.mode < 0) g_cbdata.mode = 0;
	if (g_cbdata.mode > 22) g_cbdata.mode = 22;
	g_cbdata.occluded_mode = st.mesh_chams_occluded_dx_mode;
	if (g_cbdata.occluded_mode < 0) g_cbdata.occluded_mode = 0;
	if (g_cbdata.occluded_mode > 22) g_cbdata.occluded_mode = 22;

	static const float k_white[4] = { 1.f, 1.f, 1.f, 1.f };
	const float* fill = ModeUsesFillColor(g_cbdata.mode) ? st.chams_fill_color : k_white;
	std::memcpy(g_cbdata.base_color, fill, sizeof(g_cbdata.base_color));
	std::memcpy(g_cbdata.fresnel_color, fill, sizeof(g_cbdata.fresnel_color));
	std::memcpy(g_cbdata.visible_color, fill, sizeof(g_cbdata.visible_color));

	std::memcpy(g_cbdata.occluded_color, st.mesh_chams_occluded_color,
	            sizeof(g_cbdata.occluded_color));
	std::memcpy(g_cbdata.occluded_fresnel, st.mesh_chams_occluded_color,
	            sizeof(g_cbdata.occluded_fresnel));
	g_cbdata.fresnel_power = 2.5f;
	g_cbdata.occlusion_enabled = st.mesh_chams_occlusion ? 1 : 0;

	std::memcpy(g_cbdata.outline_color, st.mesh_chams_outline_color, sizeof(g_cbdata.outline_color));
	g_cbdata.outline_fade = st.mesh_chams_outline_fade;
	if (g_cbdata.outline_fade < 0.35f) g_cbdata.outline_fade = 0.35f;
	// >2 почти не даёт вида, но раздувает radius в шейдере
	if (g_cbdata.outline_fade > 2.f) g_cbdata.outline_fade = 2.f;
	g_cbdata.outline_style = st.mesh_chams_outline_style;
	if (g_cbdata.outline_style < 0) g_cbdata.outline_style = 0;
	if (g_cbdata.outline_style > 3) g_cbdata.outline_style = 3;
	g_cbdata.outline_enabled = st.mesh_chams_outline ? 1 : 0;
	g_cbdata.glow_strength = 0.f; // pad (glow удалён)
}

void QueueMesh(const std::string& mesh_id, const Matrix4x4& world)
{
	if (!g_frame_valid)
		return;
	const GpuMesh* mesh = Fetch(mesh_id);
	if (!mesh)
		return;
	g_queue.push_back({ mesh, world });
}

void QueueBox(const Matrix4x4& world)
{
	if (!g_frame_valid || !g_unit_cube.vb)
		return;
	g_queue.push_back({ &g_unit_cube, world });
}

void Flush(ID3D11RenderTargetView* rtv)
{
	if (!g_frame_valid || !rtv || !g_context || g_queue.empty())
	{
		g_queue.clear();
		g_world_boxes.clear();
		g_frame_valid = false;
		return;
	}

	EnsureDepthFromRtv(rtv);
	if (!g_dsv)
	{
		g_queue.clear();
		g_world_boxes.clear();
		g_frame_valid = false;
		return;
	}

	const bool want_occ = g_cbdata.occlusion_enabled != 0 && g_world_dsv && g_world_srv && g_unit_cube.vb;
	if (want_occ)
	{
		// VisitOccluders читает уже готовый raycast-кэш — каждый Flush, иначе ~20fps «дёрганье»
		g_world_boxes.clear();
		g_world_boxes.reserve(900);
		Features::RaycastEngine::VisitOccluders(
			g_camera, 180.f, 900,
			[](const Vector3& pos, const Matrix4x4& rot, const Vector3& size) {
				g_world_boxes.push_back(MakeBoxWorld(pos, rot, size));
			});
	}

	// VP прямо перед GPU: BeginFrame был в начале ESP, к Flush камера уже уехала
	{
		static const uintptr_t s_base = g_Memory.GetModuleBase();
		const uintptr_t ve =
			g_Memory.Read<uintptr_t>(s_base + ::VisualEngine::Pointer);
		if (g_Memory.IsValid(ve))
		{
			const Matrix4x4 live =
				g_Memory.Read<Matrix4x4>(ve + ::VisualEngine::ViewMatrix);
			std::memcpy(g_cbdata.view, &live, sizeof(live));
		}
	}

	D3D11_VIEWPORT vp{
		0.f, 0.f, (float)g_width, (float)g_height, 0.f, 1.f
	};
	g_context->RSSetViewports(1, &vp);
	g_context->RSSetState(g_raster); // depth/occ — solid
	g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_context->IASetInputLayout(g_layout);
	g_context->VSSetShader(g_vs, nullptr, 0);
	g_context->VSSetConstantBuffers(0, 1, &g_cb);
	g_context->PSSetConstantBuffers(0, 1, &g_cb);
	g_context->OMSetDepthStencilState(g_ds, 0);

	const FLOAT bf[4]{ 0, 0, 0, 0 };

	if (want_occ && !g_world_boxes.empty())
	{
		ID3D11ShaderResourceView* null_srv[1]{ nullptr };
		g_context->PSSetShaderResources(0, 1, null_srv);

		g_context->ClearDepthStencilView(g_world_dsv, D3D11_CLEAR_DEPTH, 1.f, 0);
		ID3D11RenderTargetView* null_rtv[1]{ nullptr };
		g_context->OMSetRenderTargets(1, null_rtv, g_world_dsv);
		g_context->OMSetBlendState(g_blend_no_color, bf, 0xFFFFFFFFu);
		g_context->PSSetShader(g_ps_depth, nullptr, 0);

		for (const auto& world : g_world_boxes)
			Issue(g_unit_cube, world);

		g_context->OMSetRenderTargets(1, null_rtv, nullptr);
	}
	else if (g_cbdata.occlusion_enabled != 0)
	{
		// кэш ещё пуст — не красить всё occluded
		g_cbdata.occlusion_enabled = 0;
	}

	g_context->ClearDepthStencilView(g_dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);
	g_context->OMSetRenderTargets(1, &rtv, g_dsv);
	g_context->OMSetBlendState(g_blend, bf, 0xFFFFFFFFu);
	g_context->PSSetShader(g_ps, nullptr, 0);
	{
		const bool wire =
			g_cbdata.mode == 7 ||
			(g_cbdata.occlusion_enabled != 0 && g_cbdata.occluded_mode == 7);
		g_context->RSSetState((wire && g_raster_wire) ? g_raster_wire : g_raster);
	}

	if (g_cbdata.occlusion_enabled != 0 && g_world_srv)
	{
		ID3D11ShaderResourceView* srvs[1]{ g_world_srv };
		g_context->PSSetShaderResources(0, 1, srvs);
	}
	else
	{
		ID3D11ShaderResourceView* null_srv[1]{ nullptr };
		g_context->PSSetShaderResources(0, 1, null_srv);
	}

	for (const auto& item : g_queue)
	{
		if (item.mesh)
			Issue(*item.mesh, item.world);
	}

	// GPU outline: edge по cham depth (стены — как occluded)
	const bool want_outline =
		g_cbdata.outline_enabled != 0 && g_cham_srv && g_vs_fs && g_ps_outline;
	if (want_outline)
	{
		g_context->RSSetState(g_raster);
		ID3D11RenderTargetView* null_rtv[1]{ nullptr };
		g_context->OMSetRenderTargets(1, null_rtv, nullptr);

		ID3D11ShaderResourceView* srvs[2]{
			(g_cbdata.occlusion_enabled != 0) ? g_world_srv : g_cham_srv,
			g_cham_srv
		};
		// t0 = world (или dummy), t1 = cham
		if (g_cbdata.occlusion_enabled == 0)
			srvs[0] = g_cham_srv;
		g_context->PSSetShaderResources(0, 2, srvs);

		g_context->OMSetRenderTargets(1, &rtv, nullptr);
		g_context->OMSetDepthStencilState(g_ds_off, 0);
		g_context->OMSetBlendState(g_blend, bf, 0xFFFFFFFFu);
		g_context->VSSetShader(g_vs_fs, nullptr, 0);
		g_context->PSSetShader(g_ps_outline, nullptr, 0);
		g_context->IASetInputLayout(nullptr);
		g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// обновить CB (outline поля уже в g_cbdata)
		D3D11_MAPPED_SUBRESOURCE ms{};
		if (SUCCEEDED(g_context->Map(g_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms)))
		{
			std::memcpy(ms.pData, &g_cbdata, sizeof(g_cbdata));
			g_context->Unmap(g_cb, 0);
		}
		g_context->Draw(3, 0);

		ID3D11ShaderResourceView* null2[2]{ nullptr, nullptr };
		g_context->PSSetShaderResources(0, 2, null2);
		g_context->VSSetShader(g_vs, nullptr, 0);
		g_context->IASetInputLayout(g_layout);
		g_context->OMSetDepthStencilState(g_ds, 0);
	}
	else
	{
		ID3D11ShaderResourceView* null_srv[1]{ nullptr };
		g_context->PSSetShaderResources(0, 1, null_srv);
		g_context->OMSetRenderTargets(1, &rtv, nullptr);
	}

	g_queue.clear();
	g_world_boxes.clear();
	g_frame_valid = false;
}

const char* const* ModeNames()
{
	return k_mode_names;
}

int ModeNameCount()
{
	return (int)_countof(k_mode_names);
}

} // namespace MeshDxShader
} // namespace Visuals
} // namespace Cheat
