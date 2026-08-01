#pragma once
#include <cstdint>
#include "IdentityTag.h"

struct RenderInfo {
    Identity   entity_type = Identity::Enemy;  //实体类型（渲染层使用，OutputSystem 分层渲染顺序），复用 IdentityTag 的 Identity 枚举，避免重复定义
    float      radius      = 20.0f;              //视觉渲染半径（像素）
    uint8_t    color_r     = 255;                //颜色 R（0~255）
    uint8_t    color_g     = 255;                //颜色 G（0~255）
    uint8_t    color_b     = 255;                //颜色 B（0~255）
    uint8_t    color_a     = 255;                //颜色 Alpha（255=不透明）
};
