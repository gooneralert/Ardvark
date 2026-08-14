#pragma once

#include <d3d11.h>
#include "imgui.h"

namespace Cheat {
namespace Visuals {
namespace BoxFill {

enum ImageId : int {
    LB = 0,
    PT,
    ST,
    M2,
    OB,
    PF,
    DB,
    HR,
    VP,
    DK,
    PA,
    CB,
    KO,
    LO,
    EL,
    VK,
    EG,
    DI,
    AK,
    IC,
    PR,
    AG, // agatha_kirk
    SK,
    US,
    SE,
    COUNT
};

// картинки в дропдауне fill image (без sk/us/se)
inline constexpr int k_fill_image_count = 22;

void Init(ID3D11Device* device);
void Shutdown();
void SetRemoveBg(ID3D11Device* device, bool remove_bg);

ID3D11ShaderResourceView* Get(int id);

// растянуть картинку на бокс
void Draw(ImDrawList* dl, int id, ImVec2 min, ImVec2 max, float alpha, bool no_plate = false);

} // namespace BoxFill
} // namespace Visuals
} // namespace Cheat
