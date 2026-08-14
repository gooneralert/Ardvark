#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace Cheat::Core {

struct ModelVertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
};

struct ModelTri {
    int i0, i1, i2;
};

struct ModelPartAABB {
    float min[3];
    float max[3];
    bool  valid = false;
};

struct LoadedModel {
    std::vector<ModelVertex>   vertices;
    std::vector<ModelVertex>   wing_vertices;
    std::vector<float>         positions;
    std::vector<ModelTri>      tris;
    std::vector<ModelTri>      body_tris;
    std::vector<float>         body_positions;
    std::vector<float>         box_positions;
    std::vector<ModelPartAABB> body_parts;
    std::vector<ModelPartAABB> box_parts;
    float scale = 1.0f;
    float center[3] = {};
    float raw_min[3] = {};
    float raw_max[3] = {};
    float body_min[3] = {};
    float body_max[3] = {};
    float box_min[3] = {};
    float box_max[3] = {};
};

bool LoadOBJ(const std::string& path, LoadedModel& out);
bool LoadOBJFromMemory(const char* src, std::size_t len, LoadedModel& out);

}
