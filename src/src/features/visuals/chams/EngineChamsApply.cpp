#include "pch.h"
#include "EngineChamsApply.h"
#include "EngineChamsPriv.h"
#include "EngineChamsStyles.h"

#include "core/memory/Memory.h"

#include "core/roblox/offsets/Offsets.h"
#include "../EngineChamsOffsets.h"
#include "app/Settings.h"

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Cheat {
namespace Visuals {
namespace EngineChams {
namespace detail {

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

	return g_Memory.IsWritable(ent + ChamsOffsets::FastClusterEntity::RenderQueueId, sizeof(std::uint32_t));
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
	return g_Memory.IsWritable(layer + ChamsOffsets::MaterialLayer::FillModeByte, 1) &&
	       g_Memory.IsWritable(layer + ChamsOffsets::MaterialLayer::ColorData, sizeof(std::uint32_t));
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
			g_Memory.Write<std::uint8_t>(layer + ChamsOffsets::MaterialLayer::FillModeByte, b.fillmode);
			if (g_Memory.IsWritable(layer + ChamsOffsets::MaterialLayer::MatFlags, 4))
			{
				g_Memory.Write<std::uint32_t>(layer + ChamsOffsets::MaterialLayer::MatFlags, b.matflags);
			}
			if (g_Memory.IsWritable(layer + ChamsOffsets::MaterialLayer::Param, 4))
			{
				g_Memory.Write<std::uint32_t>(layer + ChamsOffsets::MaterialLayer::Param, b.param);
			}
			if (g_Memory.IsWritable(layer + ChamsOffsets::MaterialLayer::Flags2, 4))
			{
				g_Memory.Write<std::uint32_t>(layer + ChamsOffsets::MaterialLayer::Flags2, b.flags2);
			}
			g_Memory.Write<std::uint32_t>(layer + ChamsOffsets::MaterialLayer::ColorData, b.color);
		}

		g_layers.erase(bit);
	}
	g_ent_layers.erase(lit);
}

void RestoreAll()
{
	std::unordered_map<uintptr_t, std::uint32_t> saved;
	std::unordered_map<uintptr_t, std::vector<uintptr_t>> ent_layers;
	std::unordered_map<uintptr_t, LayerBackup> layers;
	{
		std::lock_guard<std::mutex> lk(g_mtx);
		saved.swap(g_saved);
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

		g_Memory.Write<std::uint32_t>(ent + ChamsOffsets::FastClusterEntity::RenderQueueId, id);

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
			g_Memory.Write<std::uint8_t>(layer + ChamsOffsets::MaterialLayer::FillModeByte, b.fillmode);
			if (g_Memory.IsWritable(layer + ChamsOffsets::MaterialLayer::MatFlags, 4))
			{
				g_Memory.Write<std::uint32_t>(layer + ChamsOffsets::MaterialLayer::MatFlags, b.matflags);
			}
			if (g_Memory.IsWritable(layer + ChamsOffsets::MaterialLayer::Param, 4))
			{
				g_Memory.Write<std::uint32_t>(layer + ChamsOffsets::MaterialLayer::Param, b.param);
			}
			if (g_Memory.IsWritable(layer + ChamsOffsets::MaterialLayer::Flags2, 4))
			{
				g_Memory.Write<std::uint32_t>(layer + ChamsOffsets::MaterialLayer::Flags2, b.flags2);
			}
			g_Memory.Write<std::uint32_t>(layer + ChamsOffsets::MaterialLayer::ColorData, b.color);
		}
	}
}

void RestoreOne(uintptr_t ent)
{
	if (!EntAlive(ent))
	{
		std::lock_guard<std::mutex> lk(g_mtx);
		g_saved.erase(ent);
		DropLayersLocked(ent);
		return;
	}

	std::lock_guard<std::mutex> lk(g_mtx);
	auto it = g_saved.find(ent);
	if (it != g_saved.end())
	{
		g_Memory.Write<std::uint32_t>(ent + ChamsOffsets::FastClusterEntity::RenderQueueId, it->second);
		g_saved.erase(it);
	}
	RestoreLayersLocked(ent);
}

void DropDeadLocked(uintptr_t ent)
{
	g_saved.erase(ent);
	DropLayersLocked(ent);
	g_miss.erase(ent);
}

void ApplyLayers(uintptr_t ent, std::uint8_t fill, std::uint32_t param, std::uint32_t flags2, std::uint32_t color)
{
	if (!EntAlive(ent))
	{
		return;
	}

	uintptr_t arr = g_Memory.Read<uintptr_t>(ent + ChamsOffsets::FastClusterEntity::TechniqueArrayPtr);
	if (!g_Memory.IsValid(arr))
	{
		return;
	}

	uintptr_t begin = g_Memory.Read<uintptr_t>(arr + ChamsOffsets::TechniqueArray::BeginOffset);
	uintptr_t end = g_Memory.Read<uintptr_t>(arr + ChamsOffsets::TechniqueArray::EndOffset);
	if (!g_Memory.IsValid(begin) || end <= begin)
	{
		return;
	}

	std::size_t bytes = (std::size_t)(end - begin);
	if (bytes > 64ull * 1024ull)
	{
		return;
	}

	std::size_t count = bytes / ChamsOffsets::MaterialLayer::Stride;
	if (count == 0 || count > 256)
	{
		return;
	}

	for (std::size_t i = 0; i < count; ++i)
	{
		uintptr_t layer = begin + i * ChamsOffsets::MaterialLayer::Stride;
		if (!LayerAlive(layer))
		{
			continue;
		}

		{
			std::lock_guard<std::mutex> lk(g_mtx);
			if (!g_layers.count(layer))
			{
				LayerBackup b{};
				b.fillmode = g_Memory.Read<std::uint8_t>(layer + ChamsOffsets::MaterialLayer::FillModeByte);
				b.matflags = g_Memory.Read<std::uint32_t>(layer + ChamsOffsets::MaterialLayer::MatFlags);
				b.param = g_Memory.Read<std::uint32_t>(layer + ChamsOffsets::MaterialLayer::Param);
				b.flags2 = g_Memory.Read<std::uint32_t>(layer + ChamsOffsets::MaterialLayer::Flags2);
				b.color = g_Memory.Read<std::uint32_t>(layer + ChamsOffsets::MaterialLayer::ColorData);
				g_layers[layer] = b;
				g_ent_layers[ent].push_back(layer);
			}
		}

		g_Memory.Write<std::uint8_t>(layer + ChamsOffsets::MaterialLayer::FillModeByte, fill);
		if (g_Memory.IsWritable(layer + ChamsOffsets::MaterialLayer::MatFlags, 4))
		{
			g_Memory.Write<std::uint32_t>(layer + ChamsOffsets::MaterialLayer::MatFlags, 0);
		}
		if (g_Memory.IsWritable(layer + ChamsOffsets::MaterialLayer::Param, 4))
		{
			g_Memory.Write<std::uint32_t>(layer + ChamsOffsets::MaterialLayer::Param, param);
		}
		if (g_Memory.IsWritable(layer + ChamsOffsets::MaterialLayer::Flags2, 4))
		{
			g_Memory.Write<std::uint32_t>(layer + ChamsOffsets::MaterialLayer::Flags2, flags2);
		}
		g_Memory.Write<std::uint32_t>(layer + ChamsOffsets::MaterialLayer::ColorData, color);
	}
}

void ApplyEntity(uintptr_t ent)
{
	if (!EntKnown(ent))
	{
		return;
	}

	uintptr_t rq = ent + ChamsOffsets::FastClusterEntity::RenderQueueId;
	if (!g_Memory.IsWritable(rq, sizeof(std::uint32_t)))
	{
		return;
	}

	{
		std::lock_guard<std::mutex> lk(g_mtx);
		if (!g_saved.count(ent))
		{
			g_saved[ent] = g_Memory.Read<std::uint32_t>(rq);
		}
	}

	int style = Cheat::g_Settings.esp.engine_chams_style;
	int color_idx = Cheat::g_Settings.esp.engine_ghost_color_idx;

	// движок сбрасывает queue — пишем каждый тик
	g_Memory.Write<std::uint32_t>(rq, StyleQueue(style));

	if (ApplyStyleLayers(ent, style, color_idx))
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

} // namespace detail
} // namespace EngineChams
} // namespace Visuals
} // namespace Cheat
