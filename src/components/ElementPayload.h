#pragma once
#include "ElementStatus.h"

// 投射物携带的元素载荷（发射时固化的元素类型 + 元素强度档）
// 与敌人身上的 ElementStatus 是不同概念：
//   ElementStatus = 敌人身上"附着并随时间衰减的元素状态"
//   ElementPayload = 子弹/投射物"一次性携带的攻击元素强度档（Weak/Medium/Strong）"
// 两者职责分离，符合 ECS 单一职责
struct ElementPayload {
    ElementType     element  = ElementType::None;     // 携带的元素类型（命中后挂到敌人身上）
    ElementStrength strength = ElementStrength::Weak; // 携带的元素强度档（决定附着后的 gauge/duration/decay）
};
