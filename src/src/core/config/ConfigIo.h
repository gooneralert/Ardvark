#pragma once

#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <unordered_map>

namespace Cheat {
namespace Config {
namespace detail {

using KV = std::unordered_map<std::string, std::string>;

inline void PutBool(std::ostringstream& out, const char* key, bool v)
{
    out << key << '=' << (v ? 1 : 0) << '\n';
}

inline void PutInt(std::ostringstream& out, const char* key, int v)
{
    out << key << '=' << v << '\n';
}

inline void PutFloat(std::ostringstream& out, const char* key, float v)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.6g", v);
    out << key << '=' << buf << '\n';
}

inline void PutF4(std::ostringstream& out, const char* key, const float c[4])
{
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%.6g,%.6g,%.6g,%.6g", c[0], c[1], c[2], c[3]);
    out << key << '=' << buf << '\n';
}

inline void PutF2(std::ostringstream& out, const char* key, const float c[2])
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.6g,%.6g", c[0], c[1]);
    out << key << '=' << buf << '\n';
}

inline bool GetBool(const KV& kv, const char* key, bool& out)
{
    auto it = kv.find(key);
    if (it == kv.end())
        return false;
    out = (it->second != "0" && it->second != "false");
    return true;
}

inline bool GetInt(const KV& kv, const char* key, int& out)
{
    auto it = kv.find(key);
    if (it == kv.end())
        return false;
    out = std::atoi(it->second.c_str());
    return true;
}

inline bool GetFloat(const KV& kv, const char* key, float& out)
{
    auto it = kv.find(key);
    if (it == kv.end())
        return false;
    out = (float)std::atof(it->second.c_str());
    return true;
}

inline bool GetF4(const KV& kv, const char* key, float out[4])
{
    auto it = kv.find(key);
    if (it == kv.end())
        return false;

    float a = out[0], b = out[1], c = out[2], d = out[3];
    if (sscanf_s(it->second.c_str(), "%f,%f,%f,%f", &a, &b, &c, &d) == 4)
    {
        out[0] = a;
        out[1] = b;
        out[2] = c;
        out[3] = d;
        return true;
    }
    return false;
}

inline bool GetF2(const KV& kv, const char* key, float out[2])
{
    auto it = kv.find(key);
    if (it == kv.end())
        return false;

    float a = out[0], b = out[1];
    if (sscanf_s(it->second.c_str(), "%f,%f", &a, &b) == 2)
    {
        out[0] = a;
        out[1] = b;
        return true;
    }
    return false;
}

} // namespace detail
} // namespace Config
} // namespace Cheat
