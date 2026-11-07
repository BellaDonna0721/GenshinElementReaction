#pragma once
#include "../systems/System.h"
#include "../core/Entity.h"

// =====================================================================
//  EnemyAISystem —— 敌人 AI 系统
//
//  职责：根据挂载的行为组件分派不同 AI 逻辑
//    · update_state_machine —— 四态状态机（巡逻/警戒/战斗/逃跑）
//    · update_random_patrol —— 随机巡逻（无目的漫游，不主动追击）
//
//  新增 AI 类型：新建行为组件 + 新增 update_xxx 函数，现有代码零改动。
//
//  流水线位置：MovementSystem 之前（先算速度，再移动）
// =====================================================================

class EnemyAISystem : public System {
public:
    void update(World& world, float dt) override;

private:
    // 状态机行为（需 StateMachineAI 组件）
    void update_state_machine(World& world, float dt, Entity player);
    // 随机巡逻行为（需 RandomPatrolAI 组件）
    void update_random_patrol(World& world, float dt);
};
