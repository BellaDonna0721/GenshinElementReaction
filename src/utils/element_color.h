#pragma once
#include "../components/ElementStatus.h"
#include <cstdint>

// 七元素颜色映射
//
// 用户约定：
//   1=火(Pyro)   红
//   2=水(Hydro)  深蓝
//   3=雷(Electro)紫
//   4=冰(Cryo)   蓝白
//   5=风(Anemo)   浅绿
//   6=岩(Geo)     土黄
//   7=草(Dendro)  绿

struct ElementColor { uint8_t r,g,b; };

inline ElementColor get_element_color(ElementType type) {
    switch (type) {
        case ElementType::Pyro:    return {255, 60,  40 };  // 火：红色
        case ElementType::Hydro:   return {30,  80,  230};  // 水：深蓝色
        case ElementType::Electro: return {180, 60,  255};  // 雷：紫色
        case ElementType::Cryo:    return {200, 235, 255};  // 冰：蓝白色
        case ElementType::Anemo:   return {120, 235, 175};  // 风：浅绿
        case ElementType::Geo:     return {210, 170, 70 };  // 岩：土黄色
        case ElementType::Dendro:  return {80,  205, 80 };  // 草：绿色
        case ElementType::None:    return {160, 160, 160};  // 无：灰色（兜底）
        default:                    return {160, 160, 160};
    }
}
