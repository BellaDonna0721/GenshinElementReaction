#pragma once
#include "../utils/ElementType.h"

// =====================================================================
//  ShieldPickup —— 护盾晶片道具效果组件
//
//  单一职责：存储护盾晶片的效果数据。
//  挂在 Identity=Pickup 的实体上，PickupSystem 拾取后读取并生效。
//
//  来源：岩元素结晶反应（岩 × 火/水/雷/冰）在敌人周围掉落。
//  效果：拾取后 GameState.shield_count += layers（受 shield_max 上限约束）。
//
//  预留扩展：element 字段记录对应结晶元素，未来可实现元素抗性护盾。
// =====================================================================

struct ShieldPickup {
    int         layers  = 1;                  // 提供的护盾层数
    ElementType element = ElementType::None;  // 对应结晶元素（预留元素抗性扩展）
};
