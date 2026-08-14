#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Cheat {
namespace Visuals {
namespace MeshParser {

enum class Kind : std::uint8_t {
	Body = 0,
	Accessory = 1,
	Face = 2,
	Hair = 3,
	CharacterMesh = 4,
	Special = 5,
	Other = 6
};

struct Entry {
	std::uint64_t part{ 0 };          // BasePart / Handle (рисовать)
	std::uint64_t special_mesh{ 0 };  // SpecialMesh, если есть
	std::uint64_t character{ 0 };
	Kind kind{ Kind::Other };
	std::string name;
	std::string class_name;
	std::string mesh_id;
	std::string container; // Accessory / Folder имя
};

// все визуальные меши персонажа: body + accessory/hair/face + SpecialMesh
std::vector<Entry> Collect(std::uint64_t character);

// только то, по чему можно рисовать AABB (есть part)
std::vector<Entry> CollectDrawable(std::uint64_t character);

// для bounding box: все BasePart (включая Kind::Other), без HRP/collision
std::vector<Entry> CollectForBounds(std::uint64_t character);

const char* KindName(Kind k);

} // namespace MeshParser
} // namespace Visuals
} // namespace Cheat
