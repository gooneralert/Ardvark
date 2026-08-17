#include "pch.h"
#include "InstanceCreate.h"
#include "CallGate.h"
#include "Reflect.h"
#include "core/memory/Memory.h"
#include "core/roblox/classes/Classes.h"
#include "core/roblox/offsets/Offsets.h"

#include <cstring>

namespace Cheat {
namespace Features {
namespace InstanceCreate {
namespace {

int g_last_fail = 0;

} // namespace

int LastFail()
{
	return g_last_fail;
}

// Резолвка — чтения. Сама create идёт через CallGate: аsm-стаб исполняет
// create(this, result, 0) на потоке движка. Это guide-метод:
//
//   create(this, result*, creator_role)
//     this          = ICreator (class_desc + ClassDescriptor::Creator)
//     result        = 16-байтный буфер под shared_ptr<Instance>
//     creator_role  = 0
//
// Экземпляр — первые 8 байт буфера (raw Instance*).
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

	// scan the Creators DenseHashMap (class name -> ClassDescriptor -> ICreator)
	const std::uint64_t creator = Reflect::CreatorByName(base, className);
	if (!creator)
	{
		g_last_fail = 3;
		return false;
	}

	const auto vt = g_Memory.Read<std::uint64_t>((std::uintptr_t)creator);
	if (!vt || !g_Memory.IsValid((std::uintptr_t)vt))
	{
		g_last_fail = 4;
		return false;
	}

	// ICreator::Create = vtable[0]
	const auto create_fn = g_Memory.Read<std::uint64_t>(
		(std::uintptr_t)vt + ICreator::Create);
	if (!create_fn || !g_Memory.IsValid((std::uintptr_t)create_fn))
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

	// result = 16-байтный буфер shared_ptr; обнуляем перед вызовом, чтобы при
	// неудаче отличать пустой результат от мусора.
	const std::uint8_t zero[16]{};
	g_Memory.WriteRaw((std::uintptr_t)scratch, zero, sizeof(zero));

	std::uint64_t ret = 0;
	if (!CallGate::Invoke(create_fn, creator, scratch, 0, 0, &ret))
	{
		g_last_fail = 10;
		return false;
	}

	// shared_ptr: первые 8 байт — raw Instance*. Буфер не перезаписывается,
	// чтобы shared_ptr держал ссылку и экземпляр не собрался GC.
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
