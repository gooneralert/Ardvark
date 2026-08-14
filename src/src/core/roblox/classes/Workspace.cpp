#include "pch.h"
#include "Classes.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"

std::shared_ptr<Cheat::Instance> Cheat::Workspace::GetCurrentCamera() const
{
	if (!g_Memory.IsValid(address))
	{
		return nullptr;
	}

	std::uint64_t cam = g_Memory.Read<std::uint64_t>(address + ::Workspace::CurrentCamera);
	if (!g_Memory.IsValid(cam))
	{
		return nullptr;
	}

	return std::make_shared<Cheat::Instance>(cam);
}

double Cheat::Workspace::GetDistributedGameTime() const
{
	// ::Workspace::DistributedGameTime was removed in the updated offsets header — disabled.
	(void)address;
	return 0.0;
}

float Cheat::Workspace::GetGravity() const
{
	if (!g_Memory.IsValid(address))
	{
		return 0.f;
	}

	// гравитация не в workspace а в world
	std::uint64_t world = g_Memory.Read<std::uint64_t>(address + ::Workspace::World);
	if (!g_Memory.IsValid(world))
	{
		return 0.f;
	}

	return g_Memory.Read<float>(world + ::World::Gravity);
}
