#include "pch.h"
#include "Classes.h"
#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"

std::string Cheat::MeshPart::GetMeshId() const
{
	if (!g_Memory.IsValid(address))
	{
		return "Unknown";
	}

	// Content string is embedded at MeshId (not a pointer-to-string)
	std::string s = g_Memory.ReadString(address + ::MeshPart::MeshId);
	if (!s.empty() && s != "Unknown")
		return s;

	std::uint64_t mesh = g_Memory.Read<std::uint64_t>(address + ::MeshPart::MeshId);
	if (!g_Memory.IsValid(mesh))
	{
		return "Unknown";
	}

	return g_Memory.ReadString(mesh);
}

std::string Cheat::MeshPart::GetTexture() const
{
	if (!g_Memory.IsValid(address))
	{
		return "Unknown";
	}

	std::string s = g_Memory.ReadString(address + ::MeshPart::Texture);
	if (!s.empty() && s != "Unknown")
		return s;

	std::uint64_t tex = g_Memory.Read<std::uint64_t>(address + ::MeshPart::Texture);
	if (!g_Memory.IsValid(tex))
	{
		return "Unknown";
	}

	return g_Memory.ReadString(tex);
}
