#include "main_api.hpp"
#include "parsers/mtl_parser.hpp"

c_avatar_3d_data* c_avatar_3d_api::placeholder_data()
{
    static c_avatar_3d_data dummy;
    static bool ready = false;
    if (!ready)
    {
        static const char kObj[] =
            "v -0.42 0.00 -0.18\n"
            "v  0.42 0.00 -0.18\n"
            "v  0.42 0.00  0.18\n"
            "v -0.42 0.00  0.18\n"
            "v -0.42 1.72 -0.18\n"
            "v  0.42 1.72 -0.18\n"
            "v  0.42 1.72  0.18\n"
            "v -0.42 1.72  0.18\n"
            "v -0.22 1.72 -0.16\n"
            "v  0.22 1.72 -0.16\n"
            "v  0.22 1.72  0.16\n"
            "v -0.22 1.72  0.16\n"
            "v -0.22 2.08 -0.16\n"
            "v  0.22 2.08 -0.16\n"
            "v  0.22 2.08  0.16\n"
            "v -0.22 2.08  0.16\n"
            "f 1 2 3\n"
            "f 1 3 4\n"
            "f 5 8 7\n"
            "f 5 7 6\n"
            "f 1 5 6\n"
            "f 1 6 2\n"
            "f 2 6 7\n"
            "f 2 7 3\n"
            "f 3 7 8\n"
            "f 3 8 4\n"
            "f 4 8 5\n"
            "f 4 5 1\n"
            "f 9 10 11\n"
            "f 9 11 12\n"
            "f 13 16 15\n"
            "f 13 15 14\n"
            "f 9 13 14\n"
            "f 9 14 10\n"
            "f 10 14 15\n"
            "f 10 15 11\n"
            "f 11 15 16\n"
            "f 11 16 12\n"
            "f 12 16 13\n"
            "f 12 13 9\n";
        dummy.obj_data.assign(kObj, kObj + sizeof(kObj) - 1);
        dummy.ready = true;
        dummy.aabb.min[0] = -0.42f; dummy.aabb.min[1] = 0.f; dummy.aabb.min[2] = -0.18f;
        dummy.aabb.max[0] =  0.42f; dummy.aabb.max[1] = 2.08f; dummy.aabb.max[2] =  0.18f;
        dummy.camera.position[1] = 1.0f;
        dummy.camera.position[2] = 4.0f;
        ready = true;
    }
    return &dummy;
}

c_avatar_3d_data* c_avatar_3d_api::request_data(const std::string&)
{
    return placeholder_data();
}

e_avatar_3d_load_state c_avatar_3d_api::get_state(const std::string&)
{
    return e_avatar_3d_load_state::loaded;
}

bool c_avatar_3d_api::parse_obj_model(const std::vector<unsigned char>& obj_data, c_obj_model& model)
{
    return parse_obj(obj_data, model);
}

bool c_avatar_3d_api::parse_mtl_data(const std::vector<unsigned char>& mtl_data, c_obj_model& model, const std::vector<std::string>& texture_hashes)
{
    return parse_mtl(mtl_data, model, texture_hashes);
}
