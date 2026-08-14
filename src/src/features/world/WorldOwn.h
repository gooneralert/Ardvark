#pragma once

#include <cstdint>

namespace Cheat {
namespace Features {
namespace WorldOwn {

enum class Feat : std::uint8_t
{
	None = 0,
	NoShadow,
	Time,
	Ambient,
	Outdoor,
	Brightness,
	Exposure,
	Light,
	Fog,
	Env,
	ColorShift,
	Atmosphere,
	Sky,
	Bloom,
	ColorCorr,
	ColorGrade,
	Dof,
	Terrain,
	SkyboxChanger,
};

enum class Field : std::uint8_t
{
	Shadows = 0,
	Clock,
	Sun,
	Moon,
	GradTop,
	GradBot,
	Amb,
	Outdoor,
	Bri,
	Expo,
	LightCol,
	LightDir,
	FogStart,
	FogEnd,
	FogCol,
	EnvDiff,
	EnvSpec,
	ShiftTop,
	ShiftBot,
	Atmo,
	Sky,
	Bloom,
	ColorCorr,
	ColorGrade,
	Dof,
	Terrain,
	SkyboxChanger,
	Count
};

// last enable wins — claim на включении
std::uint64_t Claim(Feat f, const Field* fields, int n);
bool Owns(Feat f, Field field);
void Release(Feat f, const Field* fields, int n);

} // namespace WorldOwn
} // namespace Features
} // namespace Cheat
