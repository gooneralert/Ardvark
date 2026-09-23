#pragma once
#include <string>
#include <vector>
#include <atomic>

struct c_decoded_texture {
    std::vector<unsigned char> pixels;
    int width = 0;
    int height = 0;
    int channels = 4;
    std::atomic<bool> ready{ false };
    float inv_width = 0.0f;
    float inv_height = 0.0f;
    inline void sample(float, float, float& r, float& g, float& b, float& a) const {
        r = g = b = a = 0.f;
    }
};

class c_texture_cache {
public:
    static c_texture_cache& get() {
        static c_texture_cache instance;
        return instance;
    }
    void request_texture(const std::string&, int, const std::vector<unsigned char>&, bool = false) {}
    c_decoded_texture* get_texture(const std::string&, int) { return nullptr; }
    void clear_user(const std::string&) {}
};
