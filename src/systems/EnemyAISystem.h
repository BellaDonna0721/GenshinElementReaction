#pragma once
#include "../systems/System.h"

// =====================================================================
//  EnemyAISystem —— 敌人 AI 状态机系统
//
//  职责：
//    · 每帧更新所有带 EnemyAI 组件的敌人
//    · 根据距离玩家远近 + 血量比例驱动状态转换
//    · 执行各状态行为（巡逻/警戒/战斗/逃跑）
//    · 战斗状态下朝玩家移动，由 CollisionSystem 处理接触伤害
//
//  流水线位置：MovementSystem 之前（先算速度，再移动）
// =====================================================================

class EnemyAISystem : public System {
public:
    void update(World& world, float dt) override;
};
