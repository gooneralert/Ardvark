#include "pch.h"
#include "Classes.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"

std::shared_ptr<Cheat::Instance> Cheat::Character::GetHumanoid() const
{
	return FindFirstChild("Humanoid");
}

std::shared_ptr<Cheat::Instance> Cheat::Character::GetRootPart() const
{
	return FindFirstChild("HumanoidRootPart");
}

std::shared_ptr<Cheat::Instance> Cheat::Character::GetHead() const
{
	// банальщина но пусть будет
	return FindFirstChild("Head");
}
