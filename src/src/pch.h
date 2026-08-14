#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
// winapi GetClassName жрёт наше имя метода
#ifdef GetClassName
#undef GetClassName
#endif

#include <d3d11.h>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <optional>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <utility>

#include "imgui.h"
