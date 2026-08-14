#include "pch.h"
#include "Reflect.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/globals/Globals.h"

#include <cstring>

namespace Cheat {
namespace Features {
namespace Reflect {
namespace {

struct Table
{
	std::uint64_t start = 0;
	std::uint64_t end = 0;
	std::uint64_t empty = 0;
};

// ключ в таблице — сырой char*, а не std::string: у длинных имён он
// смотрит в кучу, мимо объекта, так что Memory::ReadString тут не годится
bool KeyEquals(std::uint64_t key, const char* text, std::size_t len)
{
	char buf[128];
	if (len + 1 > sizeof(buf))
		return false;

	if (g_Memory.ReadRaw((std::uintptr_t)key, buf, len + 1) != len + 1)
		return false;

	return buf[len] == '\0' && std::memcmp(buf, text, len) == 0;
}

#if 0 // ===== old Reflection-table reader disabled (geeg has no Reflection) =====
bool ReadTable(std::uintptr_t at, Table* out)
{
	out->start = g_Memory.Read<std::uint64_t>(at + Offsets::Reflection::TableStart);
	out->end = g_Memory.Read<std::uint64_t>(at + Offsets::Reflection::TableEnd);
	out->empty = g_Memory.Read<std::uint64_t>(at + Offsets::Reflection::TableEmpty);

	// таблица всегда степень двойки по 16 байт на слот
	return out->start && out->end > out->start &&
	       (out->end - out->start) <= 0x400000;
}
#endif // ===== /ReadTable disabled =====

} // namespace

#if 0 // ===== old Reflection Name/Creator disabled (geeg has no Reflection); callers use the Creators map =====
// открытая адресация: хеш снаружи не посчитать, поэтому линейный проход
std::uint64_t Name(std::uintptr_t base, const char* text)
{
	if (!text || !text[0])
		return 0;

	const auto reg = g_Memory.Read<std::uint64_t>(
		base + Offsets::Reflection::NameRegistry);
	if (!reg)
		return 0;

	Table t;
	if (!ReadTable((std::uintptr_t)reg + Offsets::Reflection::NameTable, &t))
		return 0;

	const std::size_t len = std::strlen(text);

	for (std::uint64_t e = t.start; e + Offsets::Reflection::TableStride <= t.end;
	     e += Offsets::Reflection::TableStride)
	{
		const auto key = g_Memory.Read<std::uint64_t>(e);
		if (!key || key == t.empty)
			continue;

		if (!KeyEquals(key, text, len))
			continue;

		return g_Memory.Read<std::uint64_t>(e + Offsets::Reflection::EntryValue);
	}

	return 0;
}

std::uint64_t Creator(std::uintptr_t base, std::uint64_t name)
{
	if (!name)
		return 0;

	Table t;
	if (!ReadTable(base + Offsets::Reflection::CreatorTable, &t))
		return 0;

	for (std::uint64_t e = t.start; e + Offsets::Reflection::TableStride <= t.end;
	     e += Offsets::Reflection::TableStride)
	{
		if (g_Memory.Read<std::uint64_t>(e) != name)
			continue;

		return g_Memory.Read<std::uint64_t>(e + Offsets::Reflection::EntryValue);
	}

	return 0;
}
#endif // ===== old Reflection Name/Creator disabled (geeg has no Reflection) =====

// ---- new schema: Creators DenseHashMap ----
// Buckets are 16 bytes: +0x0 Name* (class name string), +0x8 ClassDescriptor*.
// Shared by CreatorByName (Instance.new) and ClassDescriptorByName (CallGate).
// We scan by the raw class name (KeyEquals handles interned / heap names).
static std::uint64_t ClassDescByName(std::uintptr_t base, const char* className)
{
	if (!className || !className[0])
		return 0;

	const auto map_start = g_Memory.Read<std::uint64_t>(
		base + ::Creator::MapStart);
	const auto map_end = g_Memory.Read<std::uint64_t>(
		base + ::Creator::MapEnd);
	if (!map_start || !map_end || map_end <= map_start)
		return 0;

	const std::size_t capacity = (map_end - map_start) / 16;
	if (capacity == 0 || capacity > 0x40000)
		return 0;

	const std::size_t len = std::strlen(className);

	for (std::size_t i = 0; i < capacity; ++i)
	{
		const auto name_ptr = g_Memory.Read<std::uint64_t>(map_start + i * 16);
		if (!name_ptr)
			continue;

		if (!KeyEquals(name_ptr, className, len))
			continue;

		const auto class_desc = g_Memory.Read<std::uint64_t>(map_start + i * 16 + 8);
		if (!class_desc || !g_Memory.IsValid((std::uintptr_t)class_desc))
			return 0;

		return class_desc;
	}

	return 0;
}

// Guide ("Instance.new"): class_desc's ICreator is embedded at
// ClassDescriptor::Creator; InstanceCreate::New reads vtable[ICreator::Create].
std::uint64_t CreatorByName(std::uintptr_t base, const char* className)
{
	const std::uint64_t class_desc = ClassDescByName(base, className);
	if (!class_desc)
		return 0;
	return class_desc + ::ClassDescriptor::Creator;
}

// Used by CallGate to reach Instance's descriptor and scan its function list.
std::uint64_t ClassDescriptorByName(std::uintptr_t base, const char* className)
{
	return ClassDescByName(base, className);
}

// Guide ("find_first_func"): scan a class descriptor's function-descriptor
// list for a named function and return its FunctionDescriptor pointer.
// Layout: ClassDescriptor::FunctionDescriptors (0xD0) -> array of slots; each
// slot's first pointer is a FunctionDescriptor; its name is at Descriptor::Name
// (0x8). The header doesn't expose the slot stride (prop_slot_t size), so that
// one constant is a best-effort guess to validate live.
std::uint64_t FindFunction(std::uintptr_t class_desc, const char* name)
{
	if (!class_desc || !name || !name[0])
		return 0;

	const auto funcs = g_Memory.Read<std::uint64_t>(
		class_desc + ::ClassDescriptor::FunctionDescriptors);
	if (!funcs || !g_Memory.IsValid((std::uintptr_t)funcs))
		return 0;

	constexpr std::size_t k_func_slot_stride = 16; // prop_slot_t size — validate live
	for (int i = 0; i < 0x1000; ++i)
	{
		const auto prop = g_Memory.Read<std::uint64_t>(
			(std::uintptr_t)funcs + (std::size_t)i * k_func_slot_stride);
		if (!prop || !g_Memory.IsValid((std::uintptr_t)prop))
			break;

		// descriptor name field (+8) holds a POINTER to the string — deref first,
		// then read (same pattern as Cheat::Instance::GetClassName).
		const auto np = g_Memory.Read<std::uint64_t>(
			(std::uintptr_t)prop + ::Descriptor::Name);
		if (np && g_Memory.IsValid((std::uintptr_t)np) &&
			g_Memory.ReadString((std::uintptr_t)np) == name)
		{
			return prop;
		}
	}

	return 0;
}

// WorldRoot's ClassDescriptor comes from the Workspace service instance; its
// "Raycast" FunctionDescriptor is found by scanning the function list, and the
// function pointer we hook lives at FunctionDescriptor::Function (+0x80).
std::uintptr_t RaycastSlot()
{
	const auto ws = Cheat::Globals::Workspace;
	if (!ws || !g_Memory.IsValid(ws->address))
		return 0;

	const auto class_desc = g_Memory.Read<std::uint64_t>(
		ws->address + ::Instance::ClassDescriptor);
	if (!class_desc || !g_Memory.IsValid((std::uintptr_t)class_desc))
		return 0;

	const auto func_desc = FindFunction((std::uintptr_t)class_desc, "Raycast");
	if (!func_desc)
		return 0;

	return (std::uintptr_t)func_desc + ::FunctionDescriptor::Function;
}

} // namespace Reflect
} // namespace Features
} // namespace Cheat
