#pragma once
#include <string>
#include <vector>
#include "parsers/obj_parser.hpp"
#include "texture/texture_cache.hpp"

struct c_avatar_3d_data {
    std::string target_id;
    struct c_camera {
        float position[3] = { 0.0f, 0.0f, 0.0f };
        float direction[3] = { 0.0f, 0.0f, -1.0f };
        float fov = 70.0f;
    } camera;
    struct c_aabb {
        float min[3] = { 0.0f, 0.0f, 0.0f };
        float max[3] = { 0.0f, 0.0f, 0.0f };
    } aabb;
    std::vector<std::string> texture_hashes;
    std::vector<unsigned char> obj_data;
    std::vector<unsigned char> mtl_data;
    std::vector<std::vector<unsigned char>> texture_data;
    bool ready = false;
};

enum class e_avatar_3d_load_state {
    not_loaded,
    loading,
    loaded,
    failed
};

class c_avatar_3d_api {
public:
    static c_avatar_3d_api& get() {
        static c_avatar_3d_api instance;
        return instance;
    }

    void initialize() {}
    c_avatar_3d_data* request_data(const std::string&);
    e_avatar_3d_load_state get_state(const std::string&);
    static void debug_log(const char*, ...) {}
    void clear_cache() {}
    void clear_user(const std::string&) {}
    c_avatar_3d_data* placeholder_data();

    bool parse_obj_model(const std::vector<unsigned char>& obj_data, c_obj_model& model);
    bool parse_mtl_data(const std::vector<unsigned char>& mtl_data, c_obj_model& model, const std::vector<std::string>& texture_hashes);
};
