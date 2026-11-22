#pragma once
#include "System.h"

// =====================================================================
//  CooldownSystem —— 统一冷却递减
//
//  流水线最前（InputSystem 之前），每帧把所有 Cooldown 组件
//  的 remaining 减去 dt。消费方只负责查 remaining、触发、重置。
//
//  为什么独立成系统：
//    · 单一事实源：所有冷却走同一递减节拍，消费方不会漏减或多减
//    · 无状态：系统本身不存任何实体状态，完全 ECS 合规
//    · 可并行：未来可按 ComponentPool 分片并行
// =====================================================================
class CooldownSystem : public System {
public:
    void update(World& world, float dt) override;
};
