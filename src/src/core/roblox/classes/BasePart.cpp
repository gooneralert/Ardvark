#include "pch.h"
#include "Classes.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"

static std::uint64_t GetPrimitive(std::uint64_t addr)
{
	return g_Memory.Read<std::uint64_t>(addr + ::BasePart::Primitive);
}

Vector3 Cheat::BasePart::GetPosition() const
{
	if (!g_Memory.IsValid(address))
	{
		return {};
	}

	std::uint64_t prim = GetPrimitive(address);
	if (!g_Memory.IsValid(prim))
	{
		return {};
	}

	return g_Memory.Read<Vector3>(prim + ::Primitive::Position);
}

Vector3 Cheat::BasePart::GetSize() const
{
	if (!g_Memory.IsValid(address))
	{
		return {};
	}

	std::uint64_t prim = GetPrimitive(address);
	if (!g_Memory.IsValid(prim))
	{
		return {};
	}

	return g_Memory.Read<Vector3>(prim + ::Primitive::Size);
}

Matrix4x4 Cheat::BasePart::GetRotation() const
{
	if (!g_Memory.IsValid(address))
	{
		return {};
	}

	std::uint64_t prim = GetPrimitive(address);
	if (!g_Memory.IsValid(prim))
	{
		return {};
	}

	// 3x3 лежит плоско, в матрицу 4x4 пихаем сами
	float rot[9];
	g_Memory.ReadRaw(prim + ::Primitive::Rotation, &rot, sizeof(rot));

	return Matrix4x4(
		rot[0], rot[1], rot[2], 0.f,
		rot[3], rot[4], rot[5], 0.f,
		rot[6], rot[7], rot[8], 0.f,
		0.f, 0.f, 0.f, 1.f
	);
}

Vector3 Cheat::BasePart::GetAssemblyLinearVelocity() const
{
	if (!g_Memory.IsValid(address))
	{
		return {};
	}

	std::uint64_t prim = GetPrimitive(address);
	if (!g_Memory.IsValid(prim))
	{
		return {};
	}

	return g_Memory.Read<Vector3>(prim + ::Primitive::AssemblyLinearVelocity);
}

Vector3 Cheat::BasePart::GetAssemblyAngularVelocity() const
{
	if (!g_Memory.IsValid(address))
	{
		return {};
	}

	std::uint64_t prim = GetPrimitive(address);
	if (!g_Memory.IsValid(prim))
	{
		return {};
	}

	return g_Memory.Read<Vector3>(prim + ::Primitive::AssemblyAngularVelocity);
}

void Cheat::BasePart::SetPosition(const Vector3& pos) const
{
	if (!g_Memory.IsValid(address))
	{
		return;
	}

	std::uint64_t prim = GetPrimitive(address);
	if (!g_Memory.IsValid(prim))
	{
		return;
	}

	g_Memory.Write<Vector3>(prim + ::Primitive::Position, pos);
	Invalidate();
}

void Cheat::BasePart::SetSize(const Vector3& size) const
{
	if (!g_Memory.IsValid(address))
	{
		return;
	}

	std::uint64_t prim = GetPrimitive(address);
	if (!g_Memory.IsValid(prim))
	{
		return;
	}

	g_Memory.Write<Vector3>(prim + ::Primitive::Size, size);
	Invalidate();
}

void Cheat::BasePart::SetAssemblyLinearVelocity(const Vector3& vel) const
{
	if (!g_Memory.IsValid(address))
	{
		return;
	}

	std::uint64_t prim = GetPrimitive(address);
	if (!g_Memory.IsValid(prim))
	{
		return;
	}

	g_Memory.Write<Vector3>(prim + ::Primitive::AssemblyLinearVelocity, vel);
}

void Cheat::BasePart::SetCanCollide(bool value) const
{
	if (!g_Memory.IsValid(address))
	{
		return;
	}

	std::uint64_t prim = GetPrimitive(address);
	if (!g_Memory.IsValid(prim))
	{
		return;
	}

	std::uint8_t flags = g_Memory.Read<std::uint8_t>(prim + ::Primitive::Flags);
	std::uint8_t bit = ::PrimitiveFlags::CanCollide;
	std::uint8_t next = flags;

	if (value)
	{
		next = flags | bit;
	}

	else
	{
		next = flags & ~bit;
	}

	// не дёргаем write если и так ок
	if (next != flags)
	{
		g_Memory.Write<std::uint8_t>(prim + ::Primitive::Flags, next);
	}
}

void Cheat::BasePart::SetAnchored(bool value) const
{
	if (!g_Memory.IsValid(address))
	{
		return;
	}

	std::uint64_t prim = GetPrimitive(address);
	if (!g_Memory.IsValid(prim))
	{
		return;
	}

	std::uint8_t flags = g_Memory.Read<std::uint8_t>(prim + ::Primitive::Flags);
	std::uint8_t bit = ::PrimitiveFlags::Anchored;
	std::uint8_t next = flags;

	if (value)
		next = flags | bit;

	else
		next = flags & ~bit;

	if (next != flags)
		g_Memory.Write<std::uint8_t>(prim + ::Primitive::Flags, next);
}

void Cheat::BasePart::SetTransparency(float value) const
{
	if (!g_Memory.IsValid(address))
	{
		return;
	}

	g_Memory.Write<float>(address + ::BasePart::Transparency, value);
	Invalidate();
}

void Cheat::BasePart::SetColor(const Color3& value) const
{
	if (!g_Memory.IsValid(address))
	{
		return;
	}

	std::uint64_t prim = GetPrimitive(address);
	if (!g_Memory.IsValid(prim))
	{
		return;
	}

	// пишем только в BasePart: Color/Transparency не живут в Primitive,
	// иначе рендер не пересчитывается — дергаем Validate
	g_Memory.Write<Color3>(address + ::BasePart::Color3, value);
	Invalidate();
}

void Cheat::BasePart::Invalidate() const
{
	if (!g_Memory.IsValid(address))
	{
		return;
	}

	std::uint64_t prim = GetPrimitive(address);
	if (!g_Memory.IsValid(prim))
	{
		return;
	}

	// дергаем Validate-флаг, чтобы движок пересчитал рендер
	const auto flags = g_Memory.Read<std::uint8_t>(prim + ::Primitive::Validate);
	g_Memory.Write<std::uint8_t>(prim + ::Primitive::Validate, flags ^ 1);
	g_Memory.Write<std::uint8_t>(prim + ::Primitive::Validate, flags);
}

Color3 Cheat::BasePart::GetColor() const
{
	if (!g_Memory.IsValid(address))
	{
		return {};
	}

	return g_Memory.Read<Color3>(address + ::BasePart::Color3);
}

float Cheat::BasePart::GetTransparency() const
{
	if (!g_Memory.IsValid(address))
	{
		return 0.f;
	}

	return g_Memory.Read<float>(address + ::BasePart::Transparency);
}

float Cheat::BasePart::GetReflectance() const
{
	if (!g_Memory.IsValid(address))
	{
		return 0.f;
	}

	return g_Memory.Read<float>(address + ::BasePart::Reflectance);
}

bool Cheat::BasePart::IsAnchored() const
{
	if (!g_Memory.IsValid(address))
	{
		return false;
	}

	std::uint64_t prim = GetPrimitive(address);
	if (!g_Memory.IsValid(prim))
	{
		return false;
	}

	std::uint8_t flags = g_Memory.Read<std::uint8_t>(prim + ::Primitive::Flags);
	return (flags & ::PrimitiveFlags::Anchored) != 0;
}

bool Cheat::BasePart::CanCollide() const
{
	if (!g_Memory.IsValid(address))
	{
		return false;
	}

	std::uint64_t prim = GetPrimitive(address);
	if (!g_Memory.IsValid(prim))
	{
		return false;
	}

	std::uint8_t flags = g_Memory.Read<std::uint8_t>(prim + ::Primitive::Flags);
	return (flags & ::PrimitiveFlags::CanCollide) != 0;
}

bool Cheat::BasePart::CastShadow() const
{
	if (!g_Memory.IsValid(address))
	{
		return false;
	}

	return g_Memory.Read<bool>(address + ::BasePart::CastShadow);
}
