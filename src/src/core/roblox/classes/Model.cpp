#include "pch.h"
#include "Classes.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"

std::shared_ptr<Cheat::Instance> Cheat::Model::GetPrimaryPart() const
{
	if (!g_Memory.IsValid(address))
	{
		return nullptr;
	}

	std::uint64_t part = g_Memory.Read<std::uint64_t>(address + ::Model::PrimaryPart);
	if (!g_Memory.IsValid(part))
	{
		return nullptr;
	}

	return std::make_shared<Cheat::Instance>(part);
}

float Cheat::Model::GetScale() const
{
	if (!g_Memory.IsValid(address))
	{
		return 1.f;
	}

	return g_Memory.Read<float>(address + ::Model::Scale);
}
