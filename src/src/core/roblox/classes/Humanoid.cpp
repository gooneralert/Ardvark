#include "pch.h"
#include "Classes.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"

float Cheat::Humanoid::GetHealth() const
{
	if (!g_Memory.IsValid(address))
	{
		return 0.f;
	}

	return g_Memory.Read<float>(address + ::Humanoid::Health);
}

float Cheat::Humanoid::GetMaxHealth() const
{
	if (!g_Memory.IsValid(address))
	{
		return 0.f;
	}

	return g_Memory.Read<float>(address + ::Humanoid::MaxHealth);
}

float Cheat::Humanoid::GetWalkSpeed() const
{
	if (!g_Memory.IsValid(address))
	{
		return 0.f;
	}

	return g_Memory.Read<float>(address + ::Humanoid::Walkspeed);
}

float Cheat::Humanoid::GetJumpPower() const
{
	if (!g_Memory.IsValid(address))
	{
		return 0.f;
	}

	return g_Memory.Read<float>(address + ::Humanoid::JumpPower);
}

float Cheat::Humanoid::GetJumpHeight() const
{
	if (!g_Memory.IsValid(address))
	{
		return 0.f;
	}

	return g_Memory.Read<float>(address + ::Humanoid::JumpHeight);
}

float Cheat::Humanoid::GetHipHeight() const
{
	if (!g_Memory.IsValid(address))
	{
		return 0.f;
	}

	return g_Memory.Read<float>(address + ::Humanoid::HipHeight);
}

Vector3 Cheat::Humanoid::GetMoveDirection() const
{
	if (!g_Memory.IsValid(address))
	{
		return {};
	}

	return g_Memory.Read<Vector3>(address + ::Humanoid::MoveDirection);
}

std::string Cheat::Humanoid::GetDisplayName() const
{
	if (!g_Memory.IsValid(address))
	{
		return "Unknown";
	}

	// humanoid displayname тоже inline rbx-string
	return g_Memory.ReadString(address + ::Humanoid::DisplayName);
}

bool Cheat::Humanoid::IsWalking() const
{
	if (!g_Memory.IsValid(address))
	{
		return false;
	}

	return g_Memory.Read<bool>(address + ::Humanoid::IsWalking);
}

bool Cheat::Humanoid::GetAutoRotate() const
{
	if (!g_Memory.IsValid(address))
	{
		return false;
	}

	return g_Memory.Read<bool>(address + ::Humanoid::AutoRotate);
}

std::int32_t Cheat::Humanoid::GetStateId() const
{
	if (!g_Memory.IsValid(address))
	{
		return -1;
	}

	// стейт через указатель, не напрямую
	std::uint64_t state = g_Memory.Read<std::uint64_t>(address + ::Humanoid::HumanoidState);
	if (!g_Memory.IsValid(state))
	{
		return -1;
	}

	return g_Memory.Read<std::int32_t>(state + ::Humanoid::HumanoidStateID);
}

bool Cheat::Humanoid::GetAutoJump() const
{
	if (!g_Memory.IsValid(address))
	{
		return false;
	}

	return g_Memory.Read<bool>(address + ::Humanoid::AutoJumpEnabled);
}

void Cheat::Humanoid::SetHealth(float value) const
{
	if (!g_Memory.IsValid(address))
	{
		return;
	}

	g_Memory.Write<float>(address + ::Humanoid::Health, value);
}

void Cheat::Humanoid::SetMaxHealth(float value) const
{
	if (!g_Memory.IsValid(address))
	{
		return;
	}

	g_Memory.Write<float>(address + ::Humanoid::MaxHealth, value);
}

void Cheat::Humanoid::SetWalkSpeed(float value) const
{
	if (!g_Memory.IsValid(address))
	{
		return;
	}

	g_Memory.Write<float>(address + ::Humanoid::Walkspeed, value);
}

void Cheat::Humanoid::SetJumpPower(float value) const
{
	if (!g_Memory.IsValid(address))
	{
		return;
	}

	g_Memory.Write<float>(address + ::Humanoid::JumpPower, value);
}

void Cheat::Humanoid::SetJumpHeight(float value) const
{
	if (!g_Memory.IsValid(address))
	{
		return;
	}

	g_Memory.Write<float>(address + ::Humanoid::JumpHeight, value);
}

std::uint64_t Cheat::Humanoid::GetRootPartAddress() const
{
	if (!g_Memory.IsValid(address))
	{
		return 0;
	}

	return g_Memory.Read<std::uint64_t>(address + ::Humanoid::HumanoidRootPart);
}

std::shared_ptr<Cheat::Instance> Cheat::Humanoid::GetRootPart() const
{
	std::uint64_t root = GetRootPartAddress();
	if (!g_Memory.IsValid(root))
	{
		return nullptr;
	}

	return std::make_shared<Cheat::Instance>(root);
}
