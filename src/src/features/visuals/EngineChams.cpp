#include "pch.h"
#include "EngineChams.h"

#include "core/memory/Memory.h"

#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/offsets/manual_offsets.h"
#include "core/roblox/math/Math.h"
#include "core/globals/Globals.h"
#include "core/player/PlayerHandler.h"
#include "app/Settings.h"

#include <Windows.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Cheat {
namespace Visuals {
namespace EngineChams {

namespace {

std::thread g_thread;
std::atomic<bool> g_run{ false };

struct LayerBackup {
	std::uint8_t fillmode = 0;
	std::uint32_t matflags = 0;
	std::uint32_t param = 0;
	std::uint32_t flags2 = 0;
	std::uint32_t color = 0;
};

std::mutex g_mtx;
std::unordered_map<uintptr_t, std::uint32_t> g_saved;
std::unordered_map<uintptr_t, std::uint8_t> g_alpha;
std::unordered_map<uintptr_t, LayerBackup> g_layers;
std::unordered_map<uintptr_t, std::vector<uintptr_t>> g_ent_layers;
std::unordered_map<uintptr_t, int> g_miss; // дохлый ent — не сразу дропаем
uintptr_t g_vt = 0;
int g_applied_style = -1;

// фаза2 StyleQueue — alpha не пишем, ток queue+layers
static constexpr int k_style_max = 7;
// для записи: vtable + writable
bool EntAlive(uintptr_t ent)
{
	if (!g_vt || !g_Memory.IsValid(ent) || !g_Memory.IsValid(ent + 8))
	{
		return false;
	}

	if (g_Memory.Read<uintptr_t>(ent) != g_vt)
	{
		return false;
	}

	return g_Memory.IsWritable(ent + ManualOffsets::FastClusterEntity::RenderQueueId, sizeof(std::uint32_t));
}

// для refresh: тока vtable, IsWritable иногда врёт и гасит чамсы
bool EntKnown(uintptr_t ent)
{
	if (!g_vt || !g_Memory.IsValid(ent))
	{
		return false;
	}

	return g_Memory.Read<uintptr_t>(ent) == g_vt;
}

bool LayerAlive(uintptr_t layer)
{
	return g_Memory.IsWritable(layer + ManualOffsets::MaterialLayer::FillModeByte, 1) &&
	       g_Memory.IsWritable(layer + ManualOffsets::MaterialLayer::ColorData, sizeof(std::uint32_t));
}

void DropLayersLocked(uintptr_t ent)
{
	auto lit = g_ent_layers.find(ent);
	if (lit == g_ent_layers.end())
	{
		return;
	}

	for (uintptr_t layer : lit->second)
	{
		g_layers.erase(layer);
	}
	g_ent_layers.erase(lit);
}

void RestoreLayersLocked(uintptr_t ent)
{
	auto lit = g_ent_layers.find(ent);
	if (lit == g_ent_layers.end())
	{
		return;
	}

	const bool alive = EntAlive(ent);

	for (uintptr_t layer : lit->second)
	{
		auto bit = g_layers.find(layer);
		if (bit == g_layers.end())
		{
			continue;
		}

		if (alive && LayerAlive(layer))
		{
			LayerBackup& b = bit->second;
			g_Memory.Write<std::uint8_t>(layer + ManualOffsets::MaterialLayer::FillModeByte, b.fillmode);
			if (g_Memory.IsWritable(layer + ManualOffsets::MaterialLayer::MatFlags, 4))
			{
				g_Memory.Write<std::uint32_t>(layer + ManualOffsets::MaterialLayer::MatFlags, b.matflags);
			}
			if (g_Memory.IsWritable(layer + ManualOffsets::MaterialLayer::Param, 4))
			{
				g_Memory.Write<std::uint32_t>(layer + ManualOffsets::MaterialLayer::Param, b.param);
			}
			if (g_Memory.IsWritable(layer + ManualOffsets::MaterialLayer::Flags2, 4))
			{
				g_Memory.Write<std::uint32_t>(layer + ManualOffsets::MaterialLayer::Flags2, b.flags2);
			}
			g_Memory.Write<std::uint32_t>(layer + ManualOffsets::MaterialLayer::ColorData, b.color);
		}

		g_layers.erase(bit);
	}
	g_ent_layers.erase(lit);
}

void RestoreAll()
{
	std::unordered_map<uintptr_t, std::uint32_t> saved;
	std::unordered_map<uintptr_t, std::uint8_t> alpha;
	std::unordered_map<uintptr_t, std::vector<uintptr_t>> ent_layers;
	std::unordered_map<uintptr_t, LayerBackup> layers;
	{
		std::lock_guard<std::mutex> lk(g_mtx);
		saved.swap(g_saved);
		alpha.swap(g_alpha);
		ent_layers.swap(g_ent_layers);
		layers.swap(g_layers);
		g_miss.clear();
		g_applied_style = -1;
	}

	for (const auto& [ent, id] : saved)
	{
		if (!EntAlive(ent))
		{
			continue;
		}

		g_Memory.Write<std::uint32_t>(ent + ManualOffsets::FastClusterEntity::RenderQueueId, id);
		auto ait = alpha.find(ent);
		if (ait != alpha.end())
		{
			uintptr_t ap = ent + ManualOffsets::FastClusterEntity::AlphaByte;
			if (g_Memory.IsWritable(ap, 1))
			{
				g_Memory.Write<std::uint8_t>(ap, ait->second);
			}
		}

		auto lit = ent_layers.find(ent);
		if (lit == ent_layers.end())
		{
			continue;
		}

		for (uintptr_t layer : lit->second)
		{
			auto bit = layers.find(layer);
			if (bit == layers.end() || !LayerAlive(layer))
			{
				continue;
			}

			LayerBackup& b = bit->second;
			g_Memory.Write<std::uint8_t>(layer + ManualOffsets::MaterialLayer::FillModeByte, b.fillmode);
			if (g_Memory.IsWritable(layer + ManualOffsets::MaterialLayer::MatFlags, 4))
			{
				g_Memory.Write<std::uint32_t>(layer + ManualOffsets::MaterialLayer::MatFlags, b.matflags);
			}
			if (g_Memory.IsWritable(layer + ManualOffsets::MaterialLayer::Param, 4))
			{
				g_Memory.Write<std::uint32_t>(layer + ManualOffsets::MaterialLayer::Param, b.param);
			}
			if (g_Memory.IsWritable(layer + ManualOffsets::MaterialLayer::Flags2, 4))
			{
				g_Memory.Write<std::uint32_t>(layer + ManualOffsets::MaterialLayer::Flags2, b.flags2);
			}
			g_Memory.Write<std::uint32_t>(layer + ManualOffsets::MaterialLayer::ColorData, b.color);
		}
	}
}

void RestoreOne(uintptr_t ent)
{
	if (!EntAlive(ent))
	{
		std::lock_guard<std::mutex> lk(g_mtx);
		g_saved.erase(ent);
		g_alpha.erase(ent);
		DropLayersLocked(ent);
		return;
	}

	std::lock_guard<std::mutex> lk(g_mtx);
	auto it = g_saved.find(ent);
	if (it != g_saved.end())
	{
		g_Memory.Write<std::uint32_t>(ent + ManualOffsets::FastClusterEntity::RenderQueueId, it->second);
		g_saved.erase(it);
	}
	auto ait = g_alpha.find(ent);
	if (ait != g_alpha.end())
	{
		uintptr_t ap = ent + ManualOffsets::FastClusterEntity::AlphaByte;
		if (g_Memory.IsWritable(ap, 1))
		{
			g_Memory.Write<std::uint8_t>(ap, ait->second);
		}
		g_alpha.erase(ait);
	}
	RestoreLayersLocked(ent);
}

bool WorldCenter(uintptr_t ent, Vector3& out)
{
	namespace FC = ManualOffsets::FastClusterEntity;

	float minx = g_Memory.Read<float>(ent + FC::BBoxMinX);
	float miny = g_Memory.Read<float>(ent + FC::BBoxMinY);
	float minz = g_Memory.Read<float>(ent + FC::BBoxMinZ);
	float maxx = g_Memory.Read<float>(ent + FC::BBoxMaxX);
	float maxy = g_Memory.Read<float>(ent + FC::BBoxMaxY);
	float maxz = g_Memory.Read<float>(ent + FC::BBoxMaxZ);
	if (!std::isfinite(minx) || !std::isfinite(maxx))
	{
		return false;
	}

	Vector3 local(
		(minx + maxx) * 0.5f,
		(miny + maxy) * 0.5f,
		(minz + maxz) * 0.5f);

	uintptr_t ctx = g_Memory.Read<uintptr_t>(ent + FC::ContextPtr);
	if (!g_Memory.IsValid(ctx))
	{
		return false;
	}

	uintptr_t pool = g_Memory.Read<uintptr_t>(ctx + FC::Context::PrimitivePoolPtr);
	if (!g_Memory.IsValid(pool))
	{
		return false;
	}

	uintptr_t base = g_Memory.Read<uintptr_t>(pool + FC::PrimitivePool::ArrayBase);
	if (!g_Memory.IsValid(base))
	{
		return false;
	}

	uintptr_t idx_ptr = g_Memory.Read<uintptr_t>(ent + FC::PrimitiveIndexArrayPtr);
	if (!g_Memory.IsValid(idx_ptr))
	{
		return false;
	}

	std::uint32_t idx = g_Memory.Read<std::uint32_t>(idx_ptr);
	if (idx > 1000000u)
	{
		return false;
	}

	uintptr_t record = base + FC::PrimitiveRecord::Stride * (uintptr_t)idx;
	if (!g_Memory.IsValid(record))
	{
		return false;
	}

	float m[9]{};
	if (g_Memory.ReadRaw(record, m, sizeof(m)) != sizeof(m))
	{
		return false;
	}

	Vector3 t = g_Memory.Read<Vector3>(record + FC::PrimitiveRecord::Translation);
	out.x = m[0] * local.x + m[1] * local.y + m[2] * local.z + t.x;
	out.y = m[3] * local.x + m[4] * local.y + m[5] * local.z + t.y;
	out.z = m[6] * local.x + m[7] * local.y + m[8] * local.z + t.z;
	return std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.z);
}

bool PushPartAddr(std::uint64_t part, std::vector<Vector3>& out)
{
	if (!g_Memory.IsValid(part))
	{
		return false;
	}

	uintptr_t prim = g_Memory.Read<uintptr_t>(part + ::BasePart::Primitive);
	if (!g_Memory.IsValid(prim))
	{
		return false;
	}

	Vector3 p = g_Memory.Read<Vector3>(prim + ::Primitive::Position);
	if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
	{
		return false;
	}

	out.push_back(p);
	return true;
}

bool PushPartPos(const std::shared_ptr<Instance>& part, std::vector<Vector3>& out)
{
	if (!part)
	{
		return false;
	}

	return PushPartAddr(part->address, out);
}

bool IsPartClass(const std::string& cls)
{
	if (cls == "Part") return true;
	if (cls == "MeshPart") return true;
	if (cls == "WedgePart") return true;
	if (cls == "CornerWedgePart") return true;
	if (cls == "TrussPart") return true;
	if (cls == "UnionOperation") return true;
	if (cls == "PartOperation") return true;
	if (cls == "Handle") return true;
	return false;
}

void PushKidsParts(const Instance& parent, std::vector<Vector3>& out, int depth)
{
	if (depth < 0 || !g_Memory.IsValid(parent.address))
	{
		return;
	}

	for (const auto& ch : parent.GetChildren())
	{
		if (!g_Memory.IsValid(ch.address))
		{
			continue;
		}

		const std::string cls = ch.GetClassName();
		if (IsPartClass(cls))
		{
			PushPartAddr(ch.address, out);
		}

		else if (depth > 0 &&
			(cls == "Model" || cls == "Folder" || cls == "Accessory" ||
			 cls == "Tool" || cls == "Accoutrement"))
		{
			PushKidsParts(ch, out, depth - 1);
		}
	}
}

std::uint64_t LocalPlayerAddr()
{
	if (!Globals::Players || !g_Memory.IsValid(Globals::Players->address))
	{
		return 0;
	}

	std::uint64_t lp = g_Memory.Read<std::uint64_t>(
		Globals::Players->address + ::Player::LocalPlayer);
	if (!g_Memory.IsValid(lp))
	{
		return 0;
	}

	return lp;
}

// все парты локал-чара — чамсы на мир, локал скипаем
bool GetLocalPartCenters(std::vector<Vector3>& out)
{
	out.clear();

	std::uint64_t lp = LocalPlayerAddr();
	if (!lp)
	{
		return false;
	}

	PlayerCache c = PlayerHandler::GetCachedPlayer(lp);
	PushPartPos(c.head, out);
	PushPartPos(c.humanoidRootPart, out);
	PushPartPos(c.upperTorso, out);
	PushPartPos(c.lowerTorso, out);
	PushPartPos(c.leftUpperArm, out);
	PushPartPos(c.leftLowerArm, out);
	PushPartPos(c.leftHand, out);
	PushPartPos(c.rightUpperArm, out);
	PushPartPos(c.rightLowerArm, out);
	PushPartPos(c.rightHand, out);
	PushPartPos(c.leftUpperLeg, out);
	PushPartPos(c.leftLowerLeg, out);
	PushPartPos(c.leftFoot, out);
	PushPartPos(c.rightUpperLeg, out);
	PushPartPos(c.rightLowerLeg, out);
	PushPartPos(c.rightFoot, out);

	std::uint64_t chara = c.character;
	if (!g_Memory.IsValid(chara))
	{
		chara = g_Memory.Read<std::uint64_t>(lp + ::Player::ModelInstance);
	}

	if (g_Memory.IsValid(chara))
	{
		PushKidsParts(Instance(chara), out, 2);
	}

	return !out.empty();
}

float MinDistSq(const Vector3& p, const std::vector<Vector3>& anchors)
{
	float best = 1.0e30f;
	for (const Vector3& a : anchors)
	{
		float d = p.DistanceSquaredTo(a);
		if (d < best)
		{
			best = d;
		}
	}
	return best;
}

// центр entity рядом с партом локала — не весь мир в 6 stub
bool IsLocalEntity(uintptr_t ent, const std::vector<Vector3>& local_a)
{
	if (local_a.empty())
	{
		return false;
	}

	Vector3 p{};
	if (!WorldCenter(ent, p))
	{
		return false;
	}

	return MinDistSq(p, local_a) <= 1.35f * 1.35f;
}

// charm: Param = idx+1
std::uint32_t ColorParam(int idx)
{
	if (idx < 0)
	{
		idx = 0;
	}

	if (idx > 6)
	{
		idx = 6;
	}

	return (std::uint32_t)(idx + 1);
}

bool StyleUsesPicker(int style)
{
	if (style == 1)
	{
		return true;
	}

	if (style == 2)
	{
		return true;
	}

	if (style == 5)
	{
		return true;
	}

	if (style == 6)
	{
		return true;
	}

	return false;
}

// aarrggbb — a всегда ff, тока rgb из пикера
std::uint32_t PackColorData(const float c[4])
{
	float r = c[0];
	float g = c[1];
	float b = c[2];

	if (r < 0.f) r = 0.f;
	if (r > 1.f) r = 1.f;
	if (g < 0.f) g = 0.f;
	if (g > 1.f) g = 1.f;
	if (b < 0.f) b = 0.f;
	if (b > 1.f) b = 1.f;

	std::uint32_t rr = (std::uint32_t)(r * 255.f + 0.5f);
	std::uint32_t gg = (std::uint32_t)(g * 255.f + 0.5f);
	std::uint32_t bb = (std::uint32_t)(b * 255.f + 0.5f);
	return (0xFFu << 24) | (rr << 16) | (gg << 8) | bb;
}

// ближайший из палитры под Param
int NearestColorIdx(const float c[4])
{
	static const float tab[7][3] = {
		{ 1.f, 0.f, 0.f },
		{ 0.f, 1.f, 0.f },
		{ 1.f, 0.50f, 0.f },
		{ 0.f, 0.50f, 1.f },
		{ 1.f, 0.f, 1.f },
		{ 0.f, 1.f, 1.f },
		{ 1.f, 1.f, 1.f },
	};

	float best = 1.0e30f;
	int bi = 6;

	for (int i = 0; i < 7; ++i)
	{
		float dr = c[0] - tab[i][0];
		float dg = c[1] - tab[i][1];
		float db = c[2] - tab[i][2];
		float d = dr * dr + dg * dg + db * db;
		if (d < best)
		{
			best = d;
			bi = i;
		}
	}

	return bi;
}

std::uint8_t PackAlphaByte(const float c[4])
{
	float a = c[3];
	if (a < 0.f) a = 0.f;
	if (a > 1.f) a = 1.f;

	std::uint8_t v = (std::uint8_t)(a * 255.f + 0.5f);
	// совсем тонкий — пропадает
	if (v < 40)
	{
		v = 40;
	}

	return v;
}

std::uint32_t StyleQueue(int style)
{
	namespace RQ = ManualOffsets::RenderQueue;

	if (style == 4)
	{
		// colored (был glass)
		return RQ::Glass;
	}

	if (style == 5)
	{
		// smoke no shadow (был glaze)
		return RQ::GlassTint;
	}

	if (style == 6)
	{
		// smoke
		return RQ::Transparent;
	}

	if (style == 7)
	{
		// invisible — OnTopWithDepth
		return RQ::OnTopWithDepth;
	}

	// 0 default / 1 ghost / 2 wire / 3 colored frame
	return RQ::AlwaysOnTop;
}

void ApplyLayers(uintptr_t ent, std::uint8_t fill, std::uint32_t param, std::uint32_t flags2, std::uint32_t color)
{
	if (!EntAlive(ent))
	{
		return;
	}

	uintptr_t arr = g_Memory.Read<uintptr_t>(ent + ManualOffsets::FastClusterEntity::TechniqueArrayPtr);
	if (!g_Memory.IsValid(arr))
	{
		return;
	}

	uintptr_t begin = g_Memory.Read<uintptr_t>(arr + ManualOffsets::TechniqueArray::BeginOffset);
	uintptr_t end = g_Memory.Read<uintptr_t>(arr + ManualOffsets::TechniqueArray::EndOffset);
	if (!g_Memory.IsValid(begin) || end <= begin)
	{
		return;
	}

	std::size_t bytes = (std::size_t)(end - begin);
	if (bytes > 64ull * 1024ull)
	{
		return;
	}

	std::size_t count = bytes / ManualOffsets::MaterialLayer::Stride;
	if (count == 0 || count > 256)
	{
		return;
	}

	for (std::size_t i = 0; i < count; ++i)
	{
		uintptr_t layer = begin + i * ManualOffsets::MaterialLayer::Stride;
		if (!LayerAlive(layer))
		{
			continue;
		}

		{
			std::lock_guard<std::mutex> lk(g_mtx);
			if (!g_layers.count(layer))
			{
				LayerBackup b{};
				b.fillmode = g_Memory.Read<std::uint8_t>(layer + ManualOffsets::MaterialLayer::FillModeByte);
				b.matflags = g_Memory.Read<std::uint32_t>(layer + ManualOffsets::MaterialLayer::MatFlags);
				b.param = g_Memory.Read<std::uint32_t>(layer + ManualOffsets::MaterialLayer::Param);
				b.flags2 = g_Memory.Read<std::uint32_t>(layer + ManualOffsets::MaterialLayer::Flags2);
				b.color = g_Memory.Read<std::uint32_t>(layer + ManualOffsets::MaterialLayer::ColorData);
				g_layers[layer] = b;
				g_ent_layers[ent].push_back(layer);
			}
		}

		g_Memory.Write<std::uint8_t>(layer + ManualOffsets::MaterialLayer::FillModeByte, fill);
		if (g_Memory.IsWritable(layer + ManualOffsets::MaterialLayer::MatFlags, 4))
		{
			g_Memory.Write<std::uint32_t>(layer + ManualOffsets::MaterialLayer::MatFlags, 0);
		}
		if (g_Memory.IsWritable(layer + ManualOffsets::MaterialLayer::Param, 4))
		{
			g_Memory.Write<std::uint32_t>(layer + ManualOffsets::MaterialLayer::Param, param);
		}
		if (g_Memory.IsWritable(layer + ManualOffsets::MaterialLayer::Flags2, 4))
		{
			g_Memory.Write<std::uint32_t>(layer + ManualOffsets::MaterialLayer::Flags2, flags2);
		}
		g_Memory.Write<std::uint32_t>(layer + ManualOffsets::MaterialLayer::ColorData, color);
	}
}

bool ApplyStyleLayers(uintptr_t ent, int style)
{
	const float* pc = Cheat::g_Settings.esp.engine_chams_color;
	int dropdown_idx = Cheat::g_Settings.esp.engine_ghost_color_idx;

	if (style == 1)
	{
		// ghost — цвет тока Param, ColorData white (rgb движок жрёт)
		ApplyLayers(ent, 0, ColorParam(NearestColorIdx(pc)), 0u, 0xFFFFFFFFu);
		return true;
	}

	if (style == 2)
	{
		// simple wireframe
		ApplyLayers(ent, 1, ColorParam(NearestColorIdx(pc)), 0u, 0xFFFFFFFFu);
		return true;
	}

	if (style == 3)
	{
		// colored frame — dropdown
		ApplyLayers(ent, 1, ColorParam(dropdown_idx), 7u, 0xFFFFFFFFu);
		return true;
	}

	if (style == 4)
	{
		// colored — dropdown
		ApplyLayers(ent, 0, ColorParam(dropdown_idx), 15u, 0xFFFFFFFFu);
		return true;
	}

	if (style == 5)
	{
		// smoke no shadow
		ApplyLayers(ent, 0, ColorParam(NearestColorIdx(pc)), 0u, 0xFFFFFFFFu);
		return true;
	}

	if (style == 6)
	{
		// smoke
		ApplyLayers(ent, 0, ColorParam(NearestColorIdx(pc)), 0u, 0xFFFFFFFFu);
		return true;
	}

	if (style == 7)
	{
		// invisible — без цвета, белый param
		ApplyLayers(ent, 0, ColorParam(6), 0u, 0xFFFFFFFFu);
		return true;
	}

	return false;
}

void ApplyEntity(uintptr_t ent)
{
	if (!EntKnown(ent))
	{
		return;
	}

	uintptr_t rq = ent + ManualOffsets::FastClusterEntity::RenderQueueId;
	uintptr_t ap = ent + ManualOffsets::FastClusterEntity::AlphaByte;
	if (!g_Memory.IsWritable(rq, sizeof(std::uint32_t)))
	{
		return;
	}

	{
		std::lock_guard<std::mutex> lk(g_mtx);
		if (!g_saved.count(ent))
		{
			g_saved[ent] = g_Memory.Read<std::uint32_t>(rq);
			g_alpha[ent] = g_Memory.Read<std::uint8_t>(ap);
		}
	}

	int style = Cheat::g_Settings.esp.engine_chams_style;

	if (style < 0)
	{
		style = 0;
	}

	if (style > k_style_max)
	{
		style = k_style_max;
	}

	// движок сбрасывает queue — пишем каждый тик. alpha не трогаем
	g_Memory.Write<std::uint32_t>(rq, StyleQueue(style));

	{
		std::lock_guard<std::mutex> lk(g_mtx);
		auto ait = g_alpha.find(ent);
		if (ait != g_alpha.end() && g_Memory.IsWritable(ap, 1))
		{
			g_Memory.Write<std::uint8_t>(ap, ait->second);
		}
	}

	if (ApplyStyleLayers(ent, style))
	{
		return;
	}

	// default: тока queue 13
	std::lock_guard<std::mutex> lk(g_mtx);
	if (g_ent_layers.count(ent))
	{
		RestoreLayersLocked(ent);
	}
}

void DropDeadLocked(uintptr_t ent)
{
	g_saved.erase(ent);
	g_alpha.erase(ent);
	DropLayersLocked(ent);
	g_miss.erase(ent);
}

void RefreshKnown()
{
	int style = Cheat::g_Settings.esp.engine_chams_style;

	std::vector<uintptr_t> ents;
	{
		std::lock_guard<std::mutex> lk(g_mtx);
		ents.reserve(g_saved.size());
		for (const auto& [ent, _] : g_saved)
		{
			ents.push_back(ent);
		}
	}

	const bool had_layers = g_applied_style >= 1 && g_applied_style <= 7;
	const bool want_layers = style >= 1 && style <= 7;
	if (had_layers && !want_layers)
	{
		for (uintptr_t ent : ents)
		{
			if (!EntKnown(ent))
			{
				continue;
			}

			std::lock_guard<std::mutex> lk(g_mtx);
			RestoreLayersLocked(ent);
		}
	}

	const bool skip_local = !Cheat::g_Settings.esp.draw_local;
	std::vector<Vector3> local_a;
	if (skip_local)
	{
		GetLocalPartCenters(local_a);
	}

	for (uintptr_t ent : ents)
	{
		if (!EntKnown(ent))
		{
			std::lock_guard<std::mutex> lk(g_mtx);
			int& n = g_miss[ent];
			++n;
			// не сразу дропаем — иначе мигает
			if (n >= 25)
			{
				DropDeadLocked(ent);
			}
			continue;
		}

		{
			std::lock_guard<std::mutex> lk(g_mtx);
			g_miss[ent] = 0;
		}

		// локал без draw local — откат, мир красим
		if (skip_local && IsLocalEntity(ent, local_a))
		{
			RestoreOne(ent);
			continue;
		}

		// ApplyEntity сам queue+layers
		ApplyEntity(ent);
	}

	g_applied_style = style;
}

void ScanOnce(uintptr_t vt)
{
	HANDLE proc = g_Memory.GetHandle();
	if (!proc)
	{
		return;
	}

	g_vt = vt;

	const bool skip_local = !Cheat::g_Settings.esp.draw_local;
	std::vector<Vector3> local_a;
	if (skip_local)
	{
		GetLocalPartCenters(local_a);
	}

	SYSTEM_INFO si{};
	GetSystemInfo(&si);

	uintptr_t max_va = reinterpret_cast<uintptr_t>(si.lpMaximumApplicationAddress);
	uintptr_t addr = reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress);

	static std::vector<std::uint8_t> buf;
	MEMORY_BASIC_INFORMATION mbi{};

	while (addr < max_va)
	{
		if (VirtualQueryEx(proc, reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)) == 0)
		{
			break;
		}

		bool readable =
			mbi.Protect == PAGE_READWRITE ||
			mbi.Protect == PAGE_EXECUTE_READWRITE ||
			mbi.Protect == PAGE_WRITECOPY ||
			mbi.Protect == PAGE_EXECUTE_WRITECOPY;

		bool ok =
			mbi.State == MEM_COMMIT &&
			mbi.Type != MEM_IMAGE &&
			readable;

		if (ok && mbi.RegionSize > 0 && mbi.RegionSize <= 512ull * 1024ull * 1024ull)
		{
			uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
			SIZE_T sz = mbi.RegionSize;

			if (buf.size() < sz)
			{
				buf.resize(sz);
			}

			SIZE_T got = 0;
			if (ReadProcessMemory(proc, reinterpret_cast<LPCVOID>(base),
			                      buf.data(), sz, &got) &&
			    got >= 16)
			{
				for (SIZE_T i = 0; i + 16 <= got; i += 8)
				{
					if (*reinterpret_cast<const uintptr_t*>(buf.data() + i) != vt)
					{
						continue;
					}

					uintptr_t node =
						*reinterpret_cast<const uintptr_t*>(buf.data() + i + 8);
					if (node < 0x10000 || node >= 0x7FFFFFFEFFFFull)
					{
						continue;
					}

					uintptr_t ent = base + i;

					// мир/чужие всегда; локал-парты тока с draw local
					if (skip_local && IsLocalEntity(ent, local_a))
					{
						continue;
					}

					ApplyEntity(ent);
				}
			}
		}

		uintptr_t next = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
		if (next <= addr)
		{
			break;
		}
		addr = next;
	}
}

bool Active()
{
	if (!Cheat::g_Settings.esp.engine_chams)
	{
		return false;
	}

	return g_Memory.IsAttached();
}

void Loop()
{
	bool was_on = false;
	int tick = 0;

	while (g_run.load(std::memory_order_relaxed))
	{
		bool on = Active();

		if (on)
		{
			if (!was_on)
			{
				std::lock_guard<std::mutex> lk(g_mtx);
				g_saved.clear();
				g_layers.clear();
				g_ent_layers.clear();
				g_miss.clear();
				g_applied_style = -1;
				was_on = true;
				tick = 0;
			}

			uintptr_t base = g_Memory.GetModuleBase(L"RobloxPlayerBeta.exe");
			if (base)
			{
				g_vt = base + ManualOffsets::FastClusterEntity::VTableRva;
			}

			// известные — каждый тик, фуллскан чаще
			RefreshKnown();

			if ((tick % 2) == 0 && base)
			{
				ScanOnce(g_vt);
			}

			++tick;
		}

		else if (was_on)
		{
			RestoreAll();
			was_on = false;
			tick = 0;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(on ? 15 : 200));
	}

	if (was_on)
	{
		RestoreAll();
	}
}

} // namespace

void Start()
{
	if (g_run.exchange(true))
	{
		return;
	}

	g_thread = std::thread(Loop);
}

void Stop()
{
	if (!g_run.exchange(false))
	{
		return;
	}

	if (g_thread.joinable())
	{
		g_thread.join();
	}
}

}
}
}
