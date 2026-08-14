#include "pch.h"
#include "InstanceCreate.h"
#include "CallGate.h"
#include "Reflect.h"
#include "core/memory/Memory.h"
#include "core/roblox/classes/Classes.h"
#include "core/roblox/offsets/Offsets.h"

#include <cstring>
#include <string>
#include <unordered_map>

namespace Cheat {
namespace Features {
namespace InstanceCreate {
namespace {

int g_last_fail = 0;

// isCreatable (ICreator vtable slot 0x10) — кешируем, ответ по классу не меняется
std::unordered_map<std::string, bool> g_creatable;
std::uintptr_t g_creatable_base = 0;

} // namespace

int LastFail()
{
	return g_last_fail;
}

// вся резолвка — чтения, но create обязан идти на потоке движка:
// внутри учётный аллокатор и flyweight-таблица имён под мьютексом
bool New(const char* className, std::uint64_t parent, std::uint64_t* out_addr)
{
	if (out_addr)
		*out_addr = 0;

	if (!className || !className[0])
	{
		g_last_fail = 1;
		return false;
	}

	const std::uintptr_t base = g_Memory.GetModuleBase(L"RobloxPlayerBeta.exe");
	if (!base)
	{
		g_last_fail = 2;
		return false;
	}

	// migrated: scan the Creators DenseHashMap (class name -> ClassDescriptor -> ICreator)
	const std::uint64_t creator = Reflect::CreatorByName(base, className);
	if (!creator)
	{
		g_last_fail = 3;
		return false;
	}

	const auto vt = g_Memory.Read<std::uint64_t>((std::uintptr_t)creator);
	if (!vt)
	{
		g_last_fail = 4;
		return false;
	}

	const auto create_fn = g_Memory.Read<std::uint64_t>(
		(std::uintptr_t)vt + ICreator::Create);
	// ICreator vtable IsCreatable slot — 0x10 (original method).
	const std::uintptr_t k_creator_isCreatable = 0x10;
	const auto creatable_fn = g_Memory.Read<std::uint64_t>(
		(std::uintptr_t)vt + k_creator_isCreatable);
	if (!create_fn || !creatable_fn)
	{
		g_last_fail = 5;
		return false;
	}

	if (!CallGate::Ready() && !CallGate::Install())
	{
		g_last_fail = 6;
		return false;
	}

	const std::uint64_t scratch = CallGate::Scratch();
	if (!scratch)
	{
		g_last_fail = 7;
		return false;
	}

	// сервисы и абстрактные классы создавать нельзя — движок бросит (original flow)
	if (g_creatable_base != base)
	{
		g_creatable.clear();
		g_creatable_base = base;
	}

	auto cached = g_creatable.find(className);
	if (cached == g_creatable.end())
	{
		std::uint64_t ok = 0;
		if (!CallGate::Invoke(creatable_fn, creator, 0, 0, 0, &ok))
		{
			g_last_fail = 8;
			return false;
		}
		cached = g_creatable.emplace(className, (ok & 0xFF) != 0).first;
	}

	if (!cached->second)
	{
		g_last_fail = 9;
		return false;
	}

	const std::uint8_t zero[16]{};
	g_Memory.WriteRaw((std::uintptr_t)scratch, zero, sizeof(zero));

	std::uint64_t ret = 0;
	if (!CallGate::Invoke(create_fn, creator, scratch, 0, 0, &ret))
	{
		g_last_fail = 10;
		return false;
	}

	const auto inst = g_Memory.Read<std::uint64_t>((std::uintptr_t)scratch);
	if (!inst || !g_Memory.IsValid((std::uintptr_t)inst))
	{
		g_last_fail = 11;
		return false;
	}

	if (parent && !SetParent(inst, parent))
	{
		g_last_fail = 12;
		return false;
	}

	if (out_addr)
		*out_addr = inst;

	g_last_fail = 0;
	return true;
}

// msvc std::string: 16 байт данных либо указателя, потом длина и ёмкость
bool SetString(std::uint64_t field, const char* text)
{
	if (!field || !g_Memory.IsValid(field) || !text)
	{
		g_last_fail = 1;
		return false;
	}

	const std::size_t len = std::strlen(text);

	if (len < 16)
	{
		char buf[16]{};
		std::memcpy(buf, text, len);
		g_Memory.WriteRaw(field, buf, sizeof(buf));
		g_Memory.Write<std::uint64_t>(field + 0x10, len);
		g_Memory.Write<std::uint64_t>(field + 0x18, 15);
		g_last_fail = 0;
		return true;
	}

	// Long strings need an in-engine allocation (Alloc::Malloc), which the geeg
	// header no longer exposes — disabled. Only the short inline path works.
	g_last_fail = 10;
	return false;
}

// Content = { int32 kind; int16 scheme; std::string uri }, строка на +0x10.
// схема ссылки кешируется в младших 4 битах и при ненулевом значении
// движок разбирать строку заново не станет — обнуляем, иначе новый
// rbxassetid:// будет искаться как старый rbxasset://
bool SetContent(std::uint64_t string_field, const char* text)
{
	if (!SetString(string_field, text))
		return false;

	const std::uint64_t content = string_field - 0x10;
	g_Memory.Write<std::int32_t>(content, (text && text[0]) ? 1 : 0);
	g_Memory.Write<std::int16_t>(content + 0x08, 0);
	return true;
}

bool SetParent(std::uint64_t inst, std::uint64_t parent)
{
	if (!g_Memory.IsValid(inst))
	{
		g_last_fail = 1;
		return false;
	}

	// parent=0 = unparent ок
	if (parent != 0 && !g_Memory.IsValid(parent))
	{
		g_last_fail = 2;
		return false;
	}

	// Geeg has no SetParent RVA; reuse the old kids-array architecture
	// (Cheat::Instance::SetParent manipulates ChildrenStart/End + Parent).
	const bool ok = Instance(inst).SetParent(parent);
	g_last_fail = ok ? 0 : 3;
	return ok;
}

} // namespace InstanceCreate
} // namespace Features
} // namespace Cheat
