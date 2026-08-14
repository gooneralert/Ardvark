#include "pch.h"
#include "Classes.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"

Color3 Cheat::Lighting::GetAmbient() const
{
	if (!g_Memory.IsValid(address))
	{
		return {};
	}

	return g_Memory.Read<Color3>(address + ::Lighting::Ambient);
}

Color3 Cheat::Lighting::GetOutdoorAmbient() const
{
	if (!g_Memory.IsValid(address))
	{
		return {};
	}

	return g_Memory.Read<Color3>(address + ::Lighting::OutdoorAmbient);
}

float Cheat::Lighting::GetBrightness() const
{
	if (!g_Memory.IsValid(address))
	{
		return 0.f;
	}

	return g_Memory.Read<float>(address + ::Lighting::Brightness);
}

float Cheat::Lighting::GetClockTime() const
{
	if (!g_Memory.IsValid(address))
	{
		return 0.f;
	}

	return g_Memory.Read<float>(address + ::Lighting::ClockTime);
}

float Cheat::Lighting::GetFogStart() const
{
	if (!g_Memory.IsValid(address))
	{
		return 0.f;
	}

	return g_Memory.Read<float>(address + ::Lighting::FogStart);
}

float Cheat::Lighting::GetFogEnd() const
{
	if (!g_Memory.IsValid(address))
	{
		return 0.f;
	}

	return g_Memory.Read<float>(address + ::Lighting::FogEnd);
}

Color3 Cheat::Lighting::GetFogColor() const
{
	if (!g_Memory.IsValid(address))
	{
		return {};
	}

	return g_Memory.Read<Color3>(address + ::Lighting::FogColor);
}

bool Cheat::Lighting::GetGlobalShadows() const
{
	// ::Lighting::GlobalShadows was removed in the updated offsets header — disabled.
	(void)address;
	return false;
}

void Cheat::Lighting::SetBrightness(float value) const
{
	if (!g_Memory.IsValid(address))
	{
		return;
	}

	g_Memory.Write<float>(address + ::Lighting::Brightness, value);
}

void Cheat::Lighting::SetClockTime(float value) const
{
	if (!g_Memory.IsValid(address))
	{
		return;
	}

	g_Memory.Write<float>(address + ::Lighting::ClockTime, value);
}

void Cheat::Lighting::SetFogStart(float value) const
{
	if (!g_Memory.IsValid(address))
	{
		return;
	}

	g_Memory.Write<float>(address + ::Lighting::FogStart, value);
}

void Cheat::Lighting::SetFogEnd(float value) const
{
	if (!g_Memory.IsValid(address))
	{
		return;
	}

	g_Memory.Write<float>(address + ::Lighting::FogEnd, value);
}

void Cheat::Lighting::SetGlobalShadows(bool value) const
{
	// ::Lighting::GlobalShadows was removed in the updated offsets header — disabled.
	(void)address;
	(void)value;
}

void Cheat::Lighting::SetAmbient(const Color3& value) const
{
	if (!g_Memory.IsValid(address))
	{
		return;
	}

	g_Memory.Write<Color3>(address + ::Lighting::Ambient, value);
}

void Cheat::Lighting::SetOutdoorAmbient(const Color3& value) const
{
	if (!g_Memory.IsValid(address))
	{
		return;
	}

	g_Memory.Write<Color3>(address + ::Lighting::OutdoorAmbient, value);
}

void Cheat::Lighting::SetFogColor(const Color3& value) const
{
	if (!g_Memory.IsValid(address))
	{
		return;
	}

	g_Memory.Write<Color3>(address + ::Lighting::FogColor, value);
}
