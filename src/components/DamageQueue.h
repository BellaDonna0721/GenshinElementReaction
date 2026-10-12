#pragma once
#include "../core/Entity.h"
#include "../utils/ElementType.h"
#include <vector>

// =====================================================================
//  DamageQueue —— 全局伤害事件队列（单例组件，挂 Entity 0）
//
//  设计动机：
//    伤害来源多样（直射、增幅反应、未来剧变反应/DoT/AoE），
//    若各系统直接改 Health 会导致扣血逻辑分散、死亡判定重复。
//    统一写入 DamageQueue，由 DamageSystem 单帧消费 → 扣血集中、来源可追溯。
//
//  写入方：CollisionSystem（直射）、ElementReactionSystem（增幅反应）
//  消费方：DamageSystem（扣血 + 死亡 + 飘字）
// =====================================================================

// 伤害事件项（不是独立组件，内聚在 DamageQueue 中）
struct DamageEvent {
    Entity       target;     // 受伤目标实体 ID
    float        amount;     // 最终伤害数值（已含反应倍率，显示时取整）
    ElementType  element;    // 伤害元素（决定飘字颜色；未来用于抗性计算）
    ReactionType reaction;   // None=直射伤害，否则对应反应类型
};

// 单例组件：所有待结算的伤害事件
struct DamageQueue {
    std::vector<DamageEvent> events;
};
