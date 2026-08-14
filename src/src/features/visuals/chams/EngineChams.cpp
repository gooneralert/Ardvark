#include "pch.h"
#include "EngineChams.h"
#include "EngineChamsPriv.h"
#include "EngineChamsApply.h"
#include "EngineChamsStyles.h"

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
#include <thread>
#include <unordered_map>
#include <vector>

namespace Cheat {
namespace Visuals {
namespace EngineChams {

namespace detail {

std::mutex g_mtx;
std::unordered_map<uintptr_t, std::uint32_t> g_saved;
std::unordered_map<uintptr_t, LayerBackup> g_layers;
std::unordered_map<uintptr_t, std::vector<uintptr_t>> g_ent_layers;
std::unordered_map<uintptr_t, int> g_miss;
uintptr_t g_vt = 0;
int g_applied_style = -1;

} // namespace detail

namespace {

using namespace detail;

std::thread g_thread;
std::atomic<bool> g_run{ false };

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

bool PushPartPos(const std::shared_ptr<Instance>& part, std::vector<Vector3>& out)
{
	if (!part || !g_Memory.IsValid(part->address))
	{
		return false;
	}

	uintptr_t prim = g_Memory.Read<uintptr_t>(
		part->address + ::BasePart::Primitive);
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

bool GetLocalAnchors(std::vector<Vector3>& out)
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
	return !out.empty();
}

void GetOtherRoots(std::vector<Vector3>& out, std::uint64_t local_addr)
{
	out.clear();
	PlayerHandler::ForEachPlayer([&](const PlayerCache& c)
	{
		if (c.address == local_addr)
		{
			return;
		}

		PushPartPos(c.humanoidRootPart, out);
		PushPartPos(c.head, out);
	});
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

bool IsLocalEntity(uintptr_t ent, const std::vector<Vector3>& local_a, const std::vector<Vector3>& other_a)
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

	float dl = MinDistSq(p, local_a);
	if (dl > 6.f * 6.f)
	{
		return false;
	}

	if (!other_a.empty())
	{
		float dout = MinDistSq(p, other_a);
		if (dout + 0.35f < dl)
		{
			return false;
		}
	}

	return true;
}

void RefreshKnown()
{
	int style = Cheat::g_Settings.esp.engine_chams_style;
	int color_idx = Cheat::g_Settings.esp.engine_ghost_color_idx;

	std::vector<uintptr_t> ents;
	{
		std::lock_guard<std::mutex> lk(g_mtx);
		ents.reserve(g_saved.size());
		for (const auto& [ent, _] : g_saved)
		{
			ents.push_back(ent);
		}
	}

	const bool had_layers = g_applied_style >= 1 && g_applied_style <= 9;
	const bool want_layers = style >= 1 && style <= 9;
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

		uintptr_t rq = ent + ManualOffsets::FastClusterEntity::RenderQueueId;
		if (!g_Memory.IsWritable(rq, sizeof(std::uint32_t)))
		{
			continue;
		}

		// каждый тик заново — иначе пропадают
		g_Memory.Write<std::uint32_t>(rq, StyleQueue(style));

		ApplyStyleLayers(ent, style, color_idx);
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

	bool skip_local = !Cheat::g_Settings.esp.draw_local;
	std::uint64_t lp = LocalPlayerAddr();

	std::vector<Vector3> local_a;
	std::vector<Vector3> other_a;
	bool have_local = GetLocalAnchors(local_a);
	GetOtherRoots(other_a, lp);

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

					// локал не красим, но Restore не трогаем —
					// иначе в упор чужие мигают (false local)
					if (skip_local && have_local && IsLocalEntity(ent, local_a, other_a))
					{
						continue;
					}

					if (skip_local && !have_local)
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
