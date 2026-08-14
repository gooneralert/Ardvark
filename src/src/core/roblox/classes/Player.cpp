#include "pch.h"
#include "Classes.h"
#include "core/memory/Memory.h"
#include "core/globals/Globals.h"
#include "core/roblox/offsets/Offsets.h"

std::string Cheat::Player::GetDisplayName() const
{
	if (!g_Memory.IsValid(address))
	{
		return "Unknown";
	}

	// 1. Direct rbx-string at DisplayName offset (+0xc0)
	std::string dn = g_Memory.ReadString(address + ::Player::DisplayName);
	if (!dn.empty() && dn != "Unknown")
		return dn;

	// 2. Pointer to rbx-string at DisplayName offset (+0xc0)
	std::uint64_t ptr = g_Memory.Read<std::uint64_t>(address + ::Player::DisplayName);
	if (g_Memory.IsValid(ptr))
	{
		std::string str = g_Memory.ReadString(ptr);
		if (!str.empty() && str != "Unknown")
			return str;
	}

	// 3. Fallback to Instance Name
	return GetName();
}

std::int64_t Cheat::Player::GetUserId() const
{
	if (!g_Memory.IsValid(address))
		return 0;

	return g_Memory.Read<std::int64_t>(address + ::Player::UserId);
}

std::int32_t Cheat::Player::GetAccountAge() const
{
	if (!g_Memory.IsValid(address))
	{
		return 0;
	}

	return g_Memory.Read<std::int32_t>(address + ::Player::AccountAge);
}

float Cheat::Player::GetMaxZoomDistance() const
{
	if (!g_Memory.IsValid(address))
	{
		return 0.f;
	}

	return g_Memory.Read<float>(address + ::Player::MaxZoomDistance);
}

float Cheat::Player::GetMinZoomDistance() const
{
	if (!g_Memory.IsValid(address))
	{
		return 0.f;
	}

	return g_Memory.Read<float>(address + ::Player::MinZoomDistance);
}

bool Cheat::Player::IsLocalPlayer() const
{
	if (!g_Memory.IsValid(address))
	{
		return false;
	}

	if (!Cheat::Globals::Players || !g_Memory.IsValid(Cheat::Globals::Players->address))
	{
		return false;
	}

	std::uint64_t local = g_Memory.Read<std::uint64_t>(
		Cheat::Globals::Players->address + ::Player::LocalPlayer);
	return local == address;
}

std::uint64_t Cheat::Player::GetTeam() const
{
	if (!g_Memory.IsValid(address))
	{
		return 0;
	}

	std::uint64_t team = g_Memory.Read<std::uint64_t>(address + ::Player::Team);
	if (!g_Memory.IsValid(team))
	{
		return 0;
	}

	return team;
}

std::uint64_t Cheat::Player::GetCharacterAddress() const
{
	if (!g_Memory.IsValid(address))
	{
		return 0;
	}

	return g_Memory.Read<std::uint64_t>(address + ::Player::ModelInstance);
}

std::shared_ptr<Cheat::Instance> Cheat::Player::GetCharacter() const
{
	std::uint64_t ch = GetCharacterAddress();
	if (!g_Memory.IsValid(ch))
	{
		return nullptr;
	}

	return std::make_shared<Cheat::Instance>(ch);
}
