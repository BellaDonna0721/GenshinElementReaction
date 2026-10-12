#pragma once
#include "../systems/System.h"

// =====================================================================
//  DamageSystem —— 伤害结算系统
//
//  职责：
//    1. 消费 DamageQueue 中的所有伤害事件，扣减目标 Health
//    2. 产生伤害飘字（写入 DamageNumberQueue，元素色 + 整数）
//    3. 收集本帧死亡的敌人，帧末统一销毁（避免遍历中删实体）
//
//  流水线位置：ElementReactionSystem 之后、OutputSystem 之前
//    （Collision 写直射伤害 → ElementReaction 写反应伤害 → DamageSystem 统一结算）
// =====================================================================

class DamageSystem : public System {
public:
    void update(World& world, float dt) override;
};
