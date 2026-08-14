#pragma once

#include <cstdint>

namespace Cheat {
namespace Features {
namespace WorldSlots {

std::uint64_t FindLighting();

// force_off = true на стопе треда (откат бэкапа)
void TickNoShadow(std::uint64_t lighting, bool force_off = false);
void TickTime(std::uint64_t lighting, bool force_off = false);
void TickAmbient(std::uint64_t lighting, bool force_off = false);
void TickOutdoor(std::uint64_t lighting, bool force_off = false);
void TickBrightness(std::uint64_t lighting, bool force_off = false);
void TickExposure(std::uint64_t lighting, bool force_off = false);
void TickLight(std::uint64_t lighting, bool force_off = false);
void TickFog(std::uint64_t lighting, bool force_off = false);
void TickEnv(std::uint64_t lighting, bool force_off = false);
void TickColorShift(std::uint64_t lighting, bool force_off = false);
void TickAtmosphere(std::uint64_t lighting, bool force_off = false);
void TickSky(std::uint64_t lighting, bool force_off = false);
void TickBloom(std::uint64_t lighting, bool force_off = false);
void TickColorCorr(std::uint64_t lighting, bool force_off = false);
void TickColorGrade(std::uint64_t lighting, bool force_off = false);
void TickDof(std::uint64_t lighting, bool force_off = false);
void TickTerrain(std::uint64_t lighting, bool force_off = false);
void TickSkyboxChanger(std::uint64_t lighting, bool force_off = false);

// имена пресетов для GUI — источник истины тут же, рядом с самими айдишниками
int SkyboxPresetCount();
const char* const* SkyboxPresetNames();

} // namespace WorldSlots
} // namespace Features
} // namespace Cheat
