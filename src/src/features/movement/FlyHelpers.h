#pragma once

#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"
#include "renderer/Renderer.h"

#include <Windows.h>
#include <cmath>
#include <cstdint>
#include <mutex>

#undef GetClassName

namespace Cheat {
namespace Features {
namespace Fly {
namespace helpers {

struct mat3
{
	float _11, _12, _13;
	float _21, _22, _23;
	float _31, _32, _33;
};

using NtWrite_t = LONG(__stdcall*)(HANDLE, PVOID, PVOID, ULONG, PULONG);
using NtRead_t  = LONG(__stdcall*)(HANDLE, PVOID, PVOID, ULONG, PULONG);

inline NtWrite_t g_nt_w = nullptr;
inline NtRead_t  g_nt_r = nullptr;

inline void grab_nt()
{
	static std::once_flag once;
	std::call_once(once, []() {
		HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
		if (!ntdll)
			ntdll = ::LoadLibraryW(L"ntdll.dll");
		if (!ntdll)
			return;
		g_nt_w = reinterpret_cast<NtWrite_t>(::GetProcAddress(ntdll, "NtWriteVirtualMemory"));
		g_nt_r = reinterpret_cast<NtRead_t>(::GetProcAddress(ntdll, "NtReadVirtualMemory"));
	});
}

inline bool raw_write(std::uint64_t addr, const void* buf, std::size_t n)
{
	grab_nt();
	HANDLE proc = g_Memory.GetHandle();
	if (!proc || !addr || !buf || !n)
		return false;

	if (g_nt_w)
	{
		LONG st = g_nt_w(
			proc,
			(PVOID)addr,
			(void*)buf,
			(ULONG)n,
			nullptr);
		if (st >= 0)
			return true;
	}

	SIZE_T wrote = 0;
	return ::WriteProcessMemory(proc, (PVOID)addr, buf, n, &wrote) != 0 && wrote == n;
}

inline bool raw_read(std::uint64_t addr, void* buf, std::size_t n)
{
	grab_nt();
	HANDLE proc = g_Memory.GetHandle();
	if (!proc || !addr || !buf || !n)
		return false;

	if (g_nt_r)
	{
		LONG st = g_nt_r(
			proc,
			(PVOID)addr,
			buf,
			(ULONG)n,
			nullptr);
		if (st >= 0)
			return true;
	}

	SIZE_T got = 0;
	return ::ReadProcessMemory(proc, (LPCVOID)addr, buf, n, &got) != 0 && got == n;
}

template <typename T>
bool poke(std::uint64_t addr, const T& v)
{
	return raw_write(addr, &v, sizeof(T));
}

template <typename T>
T peek(std::uint64_t addr)
{
	T v{};
	raw_read(addr, &v, sizeof(T));
	return v;
}

enum class kb_layout
{
	qwerty,
	azerty
};

inline kb_layout detect_layout(HWND wnd)
{
	DWORD tid = 0;
	if (wnd)
		tid = GetWindowThreadProcessId(wnd, nullptr);

	HKL lay = GetKeyboardLayout(tid);
	LANGID lid = LOWORD((UINT_PTR)lay);
	if (PRIMARYLANGID(lid) == LANG_FRENCH)
		return kb_layout::azerty;

	return kb_layout::qwerty;
}

inline bool roblox_focused()
{
	HWND wnd = Cheat::Renderer::GetGameHwnd();
	if (!wnd || !::IsWindow(wnd))
		return false;

	return ::GetForegroundWindow() == wnd;
}

inline Vector3 cam_fwd(const mat3& r)
{
	Vector3 f(-r._13, -r._23, -r._33);
	float ls = f.LengthSquared();
	if (ls < 1e-4f || !std::isfinite(ls))
		return Vector3(0.0f, 0.0f, -1.0f);
	f.Normalize();
	return f;
}

inline Vector3 cam_right(const mat3& r)
{
	Vector3 rt(-r._11, r._21, -r._31);
	float ls = rt.LengthSquared();
	if (ls < 1e-4f || !std::isfinite(ls))
		return Vector3(1.0f, 0.0f, 0.0f);
	rt.Normalize();
	return rt;
}

inline bool key_gate(int key, int mode, bool& tog, bool& was)
{
	// always / нет бинда — всегда on
	if (mode == 2 || key == 0)
		return true;

	bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
	if (mode == 1)
	{
		if (down && !was)
			tog = !tog;
		was = down;
		return tog;
	}

	was = down;
	return down;
}

inline bool set_vel(std::uint64_t prim, const Vector3& vel)
{
	if (!prim)
		return false;

	poke(prim + ::Primitive::AssemblyLinearVelocity, vel);
	Vector3 zero{};
	poke(prim + ::Primitive::AssemblyAngularVelocity, zero);
	return true;
}

inline bool zero_vel(std::uint64_t prim)
{
	const Vector3 zero{};
	return set_vel(prim, zero);
}

struct fly_snap
{
	std::uint64_t address = 0;
	std::uint64_t hrp = 0;
	std::uint64_t prim = 0;
	std::uint64_t cam = 0;
};

inline fly_snap grab_local()
{
	fly_snap s{};

	if (!Cheat::Globals::Players || !Cheat::Globals::Players->address)
		return s;

	std::uint64_t lp = peek<std::uint64_t>(
		Cheat::Globals::Players->address + ::Player::LocalPlayer);
	if (!lp)
		return s;

	std::uint64_t chara = peek<std::uint64_t>(
		lp + ::Player::ModelInstance);
	if (!chara)
		return s;

	s.address = lp;

	auto hum = Cheat::Instance(chara).FindFirstChild("Humanoid");
	if (hum && hum->address)
	{
		s.hrp = Cheat::Humanoid(hum->address).GetRootPartAddress();
	}

	if (!s.hrp)
	{
		auto hrp = Cheat::Instance(chara).FindFirstChild("HumanoidRootPart");
		if (hrp)
			s.hrp = hrp->address;
	}

	if (s.hrp)
		s.prim = peek<std::uint64_t>(s.hrp + ::BasePart::Primitive);

	if (Cheat::Globals::Workspace && Cheat::Globals::Workspace->address)
	{
		s.cam = peek<std::uint64_t>(
			Cheat::Globals::Workspace->address + ::Workspace::CurrentCamera);
	}

	return s;
}

inline void touch_ptrs(fly_snap& s)
{
	if (s.hrp)
		s.prim = peek<std::uint64_t>(s.hrp + ::BasePart::Primitive);

	if (Cheat::Globals::Workspace && Cheat::Globals::Workspace->address)
	{
		s.cam = peek<std::uint64_t>(
			Cheat::Globals::Workspace->address + ::Workspace::CurrentCamera);
	}
}

} // namespace helpers
} // namespace Fly
} // namespace Features
} // namespace Cheat
