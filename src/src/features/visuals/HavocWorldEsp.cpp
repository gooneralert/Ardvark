#include "pch.h"
#include "HavocWorldEsp.h"
#include "ShaderChams.h"
#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/classes/Classes.h"
#include "core/roblox/offsets/Offsets.h"
#include "app/Settings.h"
#undef GetClassName
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace Cheat {
namespace Visuals {
namespace HavocWorldEsp {

namespace {

// типы лута havoc
enum class LootKind : int {
    Weapon = 0,
    Mag,
    Ammo,
    Attachment,
    Medical,
    Valuable,
    Tool,
    Electronics,
    Household,
    Document,
    Building,
    Gear,
    Other,
};

struct CachedItem {
    std::uint64_t model = 0;
    std::uint64_t anchor = 0;
    std::string   name;
    LootKind      kind = LootKind::Other;
};

struct FolderCache {
    std::vector<CachedItem> items;
    std::chrono::steady_clock::time_point next_scan{};
};

FolderCache g_corpses;
FolderCache g_loot;
FolderCache g_containers;

std::unordered_map<std::string, LootKind> g_catalog;
std::chrono::steady_clock::time_point g_catalog_next{};

bool WorldToScreen(const Matrix4x4& matrix, const Vector2& dimensions,
                   const Vector3& position, Vector2& screen,
                   float scale_x, float scale_y)
{
	float w = position.x * matrix.m[3][0] + position.y * matrix.m[3][1]
		+ position.z * matrix.m[3][2] + matrix.m[3][3];
	if (w < 0.01f)
		return false;

	float x = position.x * matrix.m[0][0] + position.y * matrix.m[0][1]
		+ position.z * matrix.m[0][2] + matrix.m[0][3];
	float y = position.x * matrix.m[1][0] + position.y * matrix.m[1][1]
		+ position.z * matrix.m[1][2] + matrix.m[1][3];

	float invw = 1.0f / w;
	x *= invw;
	y *= invw;

	screen.x = ((dimensions.x * 0.5f) + (x * dimensions.x * 0.5f)) * scale_x;
	screen.y = ((dimensions.y * 0.5f) - (y * dimensions.y * 0.5f)) * scale_y;
	return true;
}

// текст с чёрной обводкой
void DrawLabel(ImDrawList* dl, ImFont* font, float fs, ImVec2 pos, ImU32 col, const char* text)
{
    const ImU32 shadow = IM_COL32(0, 0, 0, 255);
    const float x = std::floor(pos.x);
    const float y = std::floor(pos.y);
    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            if (i == 0 && j == 0) continue;
            dl->AddText(font, fs, ImVec2(x + i, y + j), shadow, text);
        }
    }
    dl->AddText(font, fs, ImVec2(x, y), col, text);
}

std::vector<ImVec2> ConvexHull(std::vector<ImVec2> pts)
{
	if (pts.size() < 3)
		return pts;

	std::sort(pts.begin(), pts.end(), [](const ImVec2& a, const ImVec2& b)
	{
		return a.x < b.x || (a.x == b.x && a.y < b.y);
	});
	auto cross = [](const ImVec2& o, const ImVec2& a, const ImVec2& b)
	{
		return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
	};

	std::vector<ImVec2> hull(pts.size() * 2);
	int k = 0;
	for (size_t i = 0; i < pts.size(); ++i)
	{
		while (k >= 2 && cross(hull[k - 2], hull[k - 1], pts[i]) <= 0.0f)
			k--;
		hull[k++] = pts[i];
	}
	for (int i = (int)pts.size() - 2, t = k + 1; i >= 0; --i)
	{
		while (k >= t && cross(hull[k - 2], hull[k - 1], pts[i]) <= 0.0f)
			k--;
		hull[k++] = pts[i];
	}

	if (k > 0)
		hull.resize((size_t)(k - 1));

	else
		hull.resize(0);
	return hull;
}

bool IsPartClass(const std::string& cls)
{
    return cls == "Part" || cls == "MeshPart" || cls == "WedgePart"
        || cls == "CornerWedgePart" || cls == "TrussPart" || cls == "UnionOperation";
}

// якорь модели, любая парта по приоритету
std::uint64_t FindAnchor(const Instance& model)
{
	static const char* prefs[] = {
		"Base", "Handle", "Safe", "collisionBox", "Bottom", "Main"
	};
	for (auto* name : prefs)
	{
		if (auto c = model.FindFirstChild(name))
		{
			if (g_Memory.IsValid(c->address) && IsPartClass(c->GetClassName()))
				return c->address;
		}
	}

	for (const auto& child : model.GetChildren())
	{
		if (!g_Memory.IsValid(child.address))
			continue;

		if (IsPartClass(child.GetClassName()))
			return child.address;
	}
	return 0;
}

std::shared_ptr<Instance> ResolvePath(Instance root, const char* const* parts, int count)
{
    auto cur = std::make_shared<Instance>(root);
    for (int i = 0; i < count; ++i) {
        if (!cur || !g_Memory.IsValid(cur->address)) return nullptr;
        cur = cur->FindFirstChild(parts[i]);
    }
    return cur;
}

std::string ToLower(std::string s)
{
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    }
    return s;
}

bool ContainsI(const std::string& hay, const char* needle)
{
    return ToLower(hay).find(ToLower(needle)) != std::string::npos;
}

LootKind FolderToKind(const std::string& folder)
{
    if (folder == "mags") return LootKind::Mag;
    if (folder == "ammo") return LootKind::Ammo;
    if (folder == "attachments") return LootKind::Attachment;
    if (folder == "medical") return LootKind::Medical;
    if (folder == "valuables") return LootKind::Valuable;
    if (folder == "tools") return LootKind::Tool;
    if (folder == "electronics") return LootKind::Electronics;
    if (folder == "households") return LootKind::Household;
    if (folder == "documents") return LootKind::Document;
    if (folder == "buildings") return LootKind::Building;
    if (folder == "gears") return LootKind::Gear;
    return LootKind::Other;
}

void IngestCatalogFolder(const Instance& folder, LootKind kind)
{
    if (!g_Memory.IsValid(folder.address)) return;
    for (const auto& child : folder.GetChildren()) {
        if (!g_Memory.IsValid(child.address)) continue;
        const std::string cls = child.GetClassName();
        if (cls == "Folder") {
            IngestCatalogFolder(child, kind);
            continue;
        }
        if (cls != "ModuleScript") continue;
        const std::string name = child.GetName();
        if (!name.empty())
            g_catalog[name] = kind;
    }
}

// каталог из RS, раз в ~8с
void RefreshCatalog()
{
    const auto now = std::chrono::steady_clock::now();
    if (now < g_catalog_next && !g_catalog.empty())
        return;
    g_catalog_next = now + std::chrono::seconds(8);
    g_catalog.clear();

    if (!Globals::InstanceDataModel.address) return;
    Instance dm(Globals::InstanceDataModel.address);
    auto rs = dm.FindFirstChild("ReplicatedStorage");
    if (!rs) return;

    static const char* k_items[] = { "Storage", "Modules", "Items" };
    auto items = ResolvePath(*rs, k_items, 3);
    if (items && g_Memory.IsValid(items->address)) {
        for (const auto& folder : items->GetChildren()) {
            if (!g_Memory.IsValid(folder.address)) continue;
            if (folder.GetClassName() != "Folder") continue;
            IngestCatalogFolder(folder, FolderToKind(folder.GetName()));
        }
    }

    static const char* k_weapons[] = { "Storage", "Modules", "Weapons" };
    auto weapons = ResolvePath(*rs, k_weapons, 3);
    if (weapons && g_Memory.IsValid(weapons->address)) {
        for (const auto& child : weapons->GetChildren()) {
            if (!g_Memory.IsValid(child.address)) continue;
            if (child.GetClassName() != "ModuleScript") continue;
            const std::string name = child.GetName();
            if (!name.empty())
                g_catalog[name] = LootKind::Weapon;
        }
    }
}

bool HasChildNamed(const Instance& node, const char* name)
{
    auto c = node.FindFirstChild(name);
    return c && g_Memory.IsValid(c->address);
}

// каталог потом data-флаги потом угадайка по имени
LootKind ClassifyModel(const Instance& model, const std::string& name)
{
	RefreshCatalog();
	auto it = g_catalog.find(name);
	if (it != g_catalog.end())
		return it->second;

	if (auto data = model.FindFirstChild("data"))
	{
		if (g_Memory.IsValid(data->address))
		{
			if (HasChildNamed(*data, "isMag"))
				return LootKind::Mag;
			if (HasChildNamed(*data, "mag") || HasChildNamed(*data, "ammoCurrent")
				|| HasChildNamed(*data, "ammoSize"))
				return LootKind::Weapon;
			if (HasChildNamed(*data, "maxAmmo") && HasChildNamed(*data, "currentAmmo"))
				return LootKind::Mag;
		}
	}
	if (HasChildNamed(model, "magsFolder"))
		return LootKind::Weapon;

	if (ContainsI(name, "magazine") || ContainsI(name, "round magazine")
		|| ContainsI(name, "drum magazine"))
		return LootKind::Mag;

	if (ContainsI(name, "suppressor") || ContainsI(name, "sight")
		|| ContainsI(name, "scope") || ContainsI(name, "foregrip")
		|| ContainsI(name, "muzzle") || ContainsI(name, "laser")
		|| ContainsI(name, "goggles") || ContainsI(name, "holographic"))
		return LootKind::Attachment;

	if (ContainsI(name, "bandage") || ContainsI(name, "medkit")
		|| ContainsI(name, "medical") || ContainsI(name, "tourniquet")
		|| ContainsI(name, "splint") || ContainsI(name, "inhaler")
		|| ContainsI(name, "bloodset") || ContainsI(name, "vitamins"))
		return LootKind::Medical;

	if (ContainsI(name, "keycard") || ContainsI(name, "gold")
		|| ContainsI(name, "diamond") || ContainsI(name, "btc")
		|| ContainsI(name, "cache key"))
		return LootKind::Valuable;

	if (ContainsI(name, "ammo") || ContainsI(name, "cartridge")
		|| ContainsI(name, "gunpowder"))
		return LootKind::Ammo;

	return LootKind::Other;
}

bool LootKindEnabled(LootKind kind)
{
	const auto& f = g_Settings.esp.loot_filter;
	switch (kind)
	{
	case LootKind::Weapon:      return f[Settings::LOOT_WEAPONS];
	case LootKind::Mag:         return f[Settings::LOOT_MAGS];
	case LootKind::Ammo:        return f[Settings::LOOT_AMMO];
	case LootKind::Attachment:  return f[Settings::LOOT_ATTACHMENTS];
	case LootKind::Medical:     return f[Settings::LOOT_MEDICAL];
	case LootKind::Valuable:    return f[Settings::LOOT_VALUABLES];
	case LootKind::Tool:        return f[Settings::LOOT_TOOLS];
	case LootKind::Electronics: return f[Settings::LOOT_ELECTRONICS];
	case LootKind::Household:   return f[Settings::LOOT_HOUSEHOLDS];
	case LootKind::Document:    return f[Settings::LOOT_DOCUMENTS];
	case LootKind::Building:
	case LootKind::Gear:
	case LootKind::Other:       return f[Settings::LOOT_OTHER];
	}
	return false;
}

void CollectFolderModels(const Instance& folder, std::vector<CachedItem>& out, bool classify)
{
    if (!g_Memory.IsValid(folder.address)) return;
    for (const auto& child : folder.GetChildren()) {
        if (!g_Memory.IsValid(child.address)) continue;
        const std::string cls = child.GetClassName();
        if (cls != "Model") continue;

        const std::string name = child.GetName();
        // X / XA.. мусорные плейсхолдеры
        if (name.empty() || name == "X" || (name.size() > 1 && name[0] == 'X'
            && (name[1] >= 'A' && name[1] <= 'Z')))
            continue;

        const std::uint64_t anchor = FindAnchor(child);
        if (!anchor) continue;

        CachedItem item;
        item.model = child.address;
        item.anchor = anchor;
        item.name = name;
        item.kind = classify ? ClassifyModel(child, name) : LootKind::Other;
        out.push_back(std::move(item));
    }
}

void RefreshCache(FolderCache& cache, const std::vector<std::shared_ptr<Instance>>& folders,
                  int interval_ms, bool classify)
{
    const auto now = std::chrono::steady_clock::now();
    if (now < cache.next_scan && !cache.items.empty())
        return;

    cache.items.clear();
    for (const auto& folder : folders) {
        if (!folder || !g_Memory.IsValid(folder->address)) continue;
        CollectFolderModels(*folder, cache.items, classify);
    }
    cache.next_scan = now + std::chrono::milliseconds(interval_ms);
}

bool ProjectObb(const BasePart& part, const Matrix4x4& view, const Vector2& viewport,
                float scale_x, float scale_y,
                std::vector<ImVec2>& hull, float& min_x, float& min_y,
                float& max_x, float& max_y, Vector2& center)
{
	Vector3 pos = part.GetPosition();
	Vector3 sz = part.GetSize();
	Matrix4x4 rot = part.GetRotation();
	if (!std::isfinite(pos.x) || !std::isfinite(sz.x))
		return false;

	Vector3 half{ sz.x * 0.5f, sz.y * 0.5f, sz.z * 0.5f };
	Vector3 corners[8] = {
		{ -half.x, -half.y, -half.z }, { -half.x, -half.y,  half.z },
		{ -half.x,  half.y, -half.z }, { -half.x,  half.y,  half.z },
		{  half.x, -half.y, -half.z }, {  half.x, -half.y,  half.z },
		{  half.x,  half.y, -half.z }, {  half.x,  half.y,  half.z }
	};

	std::vector<ImVec2> pts;
	pts.reserve(8);
	min_x = 1e9f;
	min_y = 1e9f;
	max_x = -1e9f;
	max_y = -1e9f;
	int ok = 0;
	for (const auto& c : corners)
	{
		Vector3 world{
			pos.x + (rot.m[0][0] * c.x + rot.m[0][1] * c.y + rot.m[0][2] * c.z),
			pos.y + (rot.m[1][0] * c.x + rot.m[1][1] * c.y + rot.m[1][2] * c.z),
			pos.z + (rot.m[2][0] * c.x + rot.m[2][1] * c.y + rot.m[2][2] * c.z)
		};
		Vector2 sp{};
		if (!WorldToScreen(view, viewport, world, sp, scale_x, scale_y))
			continue;
		pts.push_back(ImVec2(sp.x, sp.y));
		if (sp.x < min_x) min_x = sp.x;
		if (sp.x > max_x) max_x = sp.x;
		if (sp.y < min_y) min_y = sp.y;
		if (sp.y > max_y) max_y = sp.y;
		++ok;
	}
	if (ok < 3)
		return false;

	hull = ConvexHull(std::move(pts));
	if (hull.size() < 3)
		return false;
	center.x = (min_x + max_x) * 0.5f;
	center.y = (min_y + max_y) * 0.5f;
	return true;
}

void DrawShaderObb(ImDrawList* draw_list, const std::vector<ImVec2>& hull,
                   int shader, const float color[4])
{
    if (hull.size() < 3) return;
    std::vector<std::vector<ImVec2>> pieces{ hull };
    ShaderChams::DrawFill(draw_list, pieces, (float)ImGui::GetTime(),
                          shader, false, false, color);
    const ImU32 outline = ShaderChams::OutlineColor(shader, false, color);
    const int n = (int)hull.size();
    for (int i = 0; i < n; ++i)
        draw_list->AddLine(hull[i], hull[(i + 1) % n], outline, 1.0f);
}

void DrawWorldEntries(ImDrawList* draw_list, ImFont* font, float font_size,
                      const Matrix4x4& view, const Vector2& viewport,
                      const Vector3& cam_pos, float scale_x, float scale_y,
                      const std::vector<CachedItem>& items, const float color[4],
                      int shader, bool filter_kinds, bool draw_chams)
{
    if (items.empty()) return;

    const ImU32 name_col = IM_COL32(
        (int)(color[0] * 255.f), (int)(color[1] * 255.f),
        (int)(color[2] * 255.f), (int)(color[3] * 255.f));

    if (shader < 0) shader = 0;
    if (shader >= ShaderChams::StyleCount) shader = ShaderChams::StyleCount - 1;

    for (const auto& item : items) {
        if (filter_kinds && !LootKindEnabled(item.kind))
            continue;
        if (!g_Memory.IsValid(item.anchor)) continue;

        BasePart part(item.anchor);
        const Vector3 pos = part.GetPosition();
        if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z))
            continue;

        const float dx = pos.x - cam_pos.x;
        const float dy = pos.y - cam_pos.y;
        const float dz = pos.z - cam_pos.z;
        const float dist_studs = std::sqrt(dx * dx + dy * dy + dz * dz);
        // havoc: всегда кап 400m
        if (BeyondRange(dist_studs))
            continue;

        std::vector<ImVec2> hull;
        float min_x = 0, min_y = 0, max_x = 0, max_y = 0;
        Vector2 center{};
        bool obb_ok = ProjectObb(part, view, viewport, scale_x, scale_y,
                                 hull, min_x, min_y, max_x, max_y, center);
        if (!obb_ok)
        {
            Vector2 screen{};
            if (!WorldToScreen(view, viewport, pos, screen, scale_x, scale_y))
                continue;
            center = screen;
            min_x = max_x = screen.x;
            min_y = max_y = screen.y;
        }

        if (draw_chams && obb_ok)
            DrawShaderObb(draw_list, hull, shader, color);

        char line[192];
        // havoc: всегда метры
        std::snprintf(line, sizeof(line), "%s [%.0fm]",
                      item.name.c_str(), StudsToMeters(dist_studs));

        const ImVec2 tsz = font->CalcTextSizeA(font_size, FLT_MAX, 0.f, line);
        const float tx = std::floor((min_x + max_x) * 0.5f - tsz.x * 0.5f);
        const float ty = std::floor(min_y - tsz.y - 2.0f);
        DrawLabel(draw_list, font, font_size, ImVec2(tx, ty), name_col, line);
    }
}

} // namespace

bool IsActivePlace()
{
    if (!Globals::InstanceDataModel.address) return false;
    if (!g_Memory.IsValid(Globals::InstanceDataModel.address)) return false;
    return Globals::InstanceDataModel.GetPlaceId() == kPlaceId;
}

bool BeyondRange(float dist_studs)
{
    if (!IsActivePlace())
    {
        return false;
    }

    return StudsToMeters(dist_studs) > kMaxMeters;
}

// world esp для havoc (трупы / лут / контейнеры)
void Render(ImDrawList* draw_list, ImFont* font, float font_size,
            const Matrix4x4& view, const Vector2& viewport,
            const Vector3& cam_pos, float overlay_w, float overlay_h,
            float scale_x, float scale_y)
{
	(void)overlay_w;
	(void)overlay_h;

	if (!IsActivePlace())
		return;
	if (!g_Settings.esp.enabled)
		return;
	if (!g_Settings.esp.corpses && !g_Settings.esp.ground_loot && !g_Settings.esp.containers)
		return;
	if (!Globals::Workspace || !g_Memory.IsValid(Globals::Workspace->address))
		return;
	if (!draw_list || !font)
		return;

	static const char* loots_path[] = { "Buildings", "Loots" };
	auto loots = ResolvePath(*Globals::Workspace, loots_path, 2);
	if (!loots || !g_Memory.IsValid(loots->address))
		return;

	if (g_Settings.esp.corpses)
	{
		std::vector<std::shared_ptr<Instance>> folders;
		static const char* chars_path[] = { "Loots", "Characters" };
		static const char* bodies_path[] = { "Bodies" };
		if (auto chars = ResolvePath(*loots, chars_path, 2))
			folders.push_back(chars);
		if (auto bodies = ResolvePath(*loots, bodies_path, 1))
			folders.push_back(bodies);
		RefreshCache(g_corpses, folders, 400, false);
		DrawWorldEntries(draw_list, font, font_size, view, viewport, cam_pos, scale_x, scale_y,
		                 g_corpses.items, g_Settings.esp.corpse_color,
		                 g_Settings.esp.loot_chams_shader, false, false);
	}

	if (g_Settings.esp.ground_loot)
	{
		std::vector<std::shared_ptr<Instance>> folders;
		for (const auto& child : loots->GetChildren())
		{
			if (!g_Memory.IsValid(child.address))
				continue;
			if (child.GetName() == "Items" && child.GetClassName() == "Folder")
			{
				folders.push_back(std::make_shared<Instance>(child));
				break;
			}
		}
		RefreshCache(g_loot, folders, 500, true);
		DrawWorldEntries(draw_list, font, font_size, view, viewport, cam_pos, scale_x, scale_y,
		                 g_loot.items, g_Settings.esp.ground_loot_color,
		                 g_Settings.esp.loot_chams_shader, true, g_Settings.esp.loot_chams);
	}

	if (g_Settings.esp.containers)
	{
		std::vector<std::shared_ptr<Instance>> folders;
		static const char* crates_path[] = { "Loots", "Crates" };
		static const char* stashes_path[] = { "Loots", "Stashes" };
		if (auto crates = ResolvePath(*loots, crates_path, 2))
			folders.push_back(crates);
		if (auto stashes = ResolvePath(*loots, stashes_path, 2))
			folders.push_back(stashes);
		RefreshCache(g_containers, folders, 800, false);
		DrawWorldEntries(draw_list, font, font_size, view, viewport, cam_pos, scale_x, scale_y,
		                 g_containers.items, g_Settings.esp.containers_color,
		                 g_Settings.esp.containers_chams_shader, false,
		                 g_Settings.esp.containers_chams);
	}
}

} // namespace HavocWorldEsp
} // namespace Visuals
} // namespace Cheat
