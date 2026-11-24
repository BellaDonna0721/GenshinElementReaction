#pragma once
#include <cstdint>

struct RenderInfo {
    float      radius      = 20.0f;              //视觉渲染半径（像素）
    uint8_t    color_r     = 255;                //颜色 R（0~255）
    uint8_t    color_g     = 255;                //颜色 G（0~255）
    uint8_t    color_b     = 255;                //颜色 B（0~255）
    uint8_t    color_a     = 255;                //颜色 Alpha（255=不透明）
};
