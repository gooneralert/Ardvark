#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Cheat {
namespace Features {
namespace AvatarLoader {

// One textured material of the downloaded avatar model. `png` holds the raw
// texture bytes (PNG). Empty `png` means the material is untextured and the
// renderer falls back to the Kd diffuse colour.
struct Material {
    std::string name;               // MTL material name (OBJ "usemtl" key)
    std::vector<unsigned char> png; // texture data (PNG) or empty
    float kd[3]{ 1.f, 1.f, 1.f };   // diffuse colour fallback
};

struct Model {
    std::string obj;                 // OBJ source (gzip already decoded)
    std::vector<Material> materials; // MTL materials matched by the OBJ by name
    bool ok = false;
};

// Download the LOCAL player's avatar in a background thread. Safe to call
// multiple times; each call (re)starts the fetch. Called once on launch.
void Start();

// Render-thread pickup: hands over the freshly downloaded avatar exactly once.
bool ConsumeReady(Model& out);

// True when the last fetch ended with a permanent failure, so the preview
// keeps using the built-in (premade) model.
bool IsFailed();

// Stop the in-flight fetch and forget downloaded data (used on reattach).
void Reset();

} } }