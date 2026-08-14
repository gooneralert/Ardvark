#include "pch.h"
#include "Classes.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"

std::uint64_t Cheat::DataModel::GetGameId() const
{
	if (!g_Memory.IsValid(address))
	{
		return 0;
	}

	return g_Memory.Read<std::uint64_t>(address + ::DataModel::GameId);
}

std::uint64_t Cheat::DataModel::GetPlaceId() const
{
	if (!g_Memory.IsValid(address))
	{
		return 0;
	}

	return g_Memory.Read<std::uint64_t>(address + ::DataModel::PlaceId);
}

std::uint64_t Cheat::DataModel::GetCreatorId() const
{
	if (!g_Memory.IsValid(address))
	{
		return 0;
	}

	return g_Memory.Read<std::uint64_t>(address + ::DataModel::CreatorId);
}

std::int32_t Cheat::DataModel::GetPlaceVersion() const
{
	// ::DataModel::PlaceVersion was removed in the updated offsets header — disabled.
	(void)address;
	return 0;
}

std::string Cheat::DataModel::GetJobId() const
{
	if (!g_Memory.IsValid(address))
	{
		return "Unknown";
	}

	std::uint64_t job = g_Memory.Read<std::uint64_t>(address + ::DataModel::JobId);
	if (!g_Memory.IsValid(job))
	{
		return "Unknown";
	}

	return g_Memory.ReadString(job);
}

bool Cheat::DataModel::IsGameLoaded() const
{
	if (!g_Memory.IsValid(address))
	{
		return false;
	}

	return g_Memory.Read<bool>(address + ::DataModel::GameLoaded);
}
