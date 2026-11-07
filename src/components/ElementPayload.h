#pragma once
#include "../utils/ElementType.h"

// =====================================================================
//  子弹攻击载荷
//
//  承载子弹的全部攻击属性：基础伤害 + 元素类型 + 元素强度档。
// =====================================================================
struct ElementPayload {
    float           base_damage = 10.0f;                 // 单次命中基础伤害（未含反应倍率）
    ElementType     element     = ElementType::None;     // 携带的元素类型（命中后挂到敌人身上）
    ElementStrength strength    = ElementStrength::Weak; // 携带的元素强度档（决定附着后的 gauge/duration/decay）
};
