#pragma once

#include "core/memory/Memory.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/classes/Classes.h"
#include "core/roblox/math/Math.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace Cheat {
namespace ValueBase {

inline bool IsClass(const std::string& cls)
{
	return cls == "BoolValue" || cls == "IntValue" || cls == "NumberValue"
		|| cls == "StringValue" || cls == "ObjectValue" || cls == "Vector3Value";
}

inline std::uint64_t Field(std::uint64_t addr)
{
	return addr + ::Misc::Value;
}

inline bool GetBool(std::uint64_t addr)
{
	return g_Memory.Read<std::uint8_t>(Field(addr)) != 0;
}

inline void SetBool(std::uint64_t addr, bool v)
{
	g_Memory.Write<std::uint8_t>(Field(addr), v ? 1 : 0);
}

inline std::int32_t GetInt(std::uint64_t addr)
{
	return g_Memory.Read<std::int32_t>(Field(addr));
}

inline void SetInt(std::uint64_t addr, std::int32_t v)
{
	g_Memory.Write<std::int32_t>(Field(addr), v);
}

inline double GetNumber(std::uint64_t addr)
{
	return g_Memory.Read<double>(Field(addr));
}

inline void SetNumber(std::uint64_t addr, double v)
{
	g_Memory.Write<double>(Field(addr), v);
}

inline std::string GetString(std::uint64_t addr)
{
	return g_Memory.ReadString(Field(addr));
}

inline bool SetString(std::uint64_t addr, const std::string& s)
{
	if (s.size() >= 16)
		return false;
	char buf[24]{};
	std::memcpy(buf, s.data(), s.size());
	*reinterpret_cast<std::int32_t*>(buf + 0x10) = static_cast<std::int32_t>(s.size());
	return g_Memory.WriteRaw(static_cast<uintptr_t>(Field(addr)), buf, 0x18) == 0x18;
}

inline std::uint64_t GetObject(std::uint64_t addr)
{
	return g_Memory.Read<std::uint64_t>(Field(addr));
}

inline void SetObject(std::uint64_t addr, std::uint64_t obj)
{
	g_Memory.Write<std::uint64_t>(Field(addr), obj);
}

inline Vector3 GetVector3(std::uint64_t addr)
{
	return g_Memory.Read<Vector3>(Field(addr));
}

inline void SetVector3(std::uint64_t addr, const Vector3& v)
{
	g_Memory.Write<Vector3>(Field(addr), v);
}

inline std::string Format(std::uint64_t addr, const std::string& cls)
{
	if (!addr || !IsClass(cls))
		return {};

	char buf[96]{};
	if (cls == "BoolValue")
		return GetBool(addr) ? "true" : "false";
	if (cls == "IntValue")
	{
		std::snprintf(buf, sizeof(buf), "%d", GetInt(addr));
		return buf;
	}
	if (cls == "NumberValue")
	{
		std::snprintf(buf, sizeof(buf), "%.6g", GetNumber(addr));
		return buf;
	}
	if (cls == "StringValue")
	{
		std::string s = GetString(addr);
		if (s.size() > 40)
			s = s.substr(0, 40) + "...";
		return "\"" + s + "\"";
	}
	if (cls == "ObjectValue")
	{
		const std::uint64_t obj = GetObject(addr);
		if (!obj || !g_Memory.IsValid(obj))
			return "nil";
		::Cheat::Instance inst(obj);
		std::string nm = inst.GetName();
		std::string c = inst.GetClassName();
		if (nm.empty())
			nm = "?";
		return nm + " (" + c + ")";
	}
	if (cls == "Vector3Value")
	{
		Vector3 v = GetVector3(addr);
		std::snprintf(buf, sizeof(buf), "%.3g, %.3g, %.3g", v.x, v.y, v.z);
		return buf;
	}
	return {};
}

} // namespace ValueBase
} // namespace Cheat
