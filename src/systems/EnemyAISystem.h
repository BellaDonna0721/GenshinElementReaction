#pragma once
#include "../systems/System.h"
#include "../core/Entity.h"

// =====================================================================
//  EnemyAISystem —— 敌人 AI 系统
//
//  职责：根据挂载的行为组件分派不同 AI 逻辑
//    · update_state_machine —— 三态状态机（巡逻/追击/逃跑，Boss 专用）
//    · update_random_patrol —— 随机巡逻（无目的漫游，不主动追击，普通小怪）
//
//  Boss 追击依赖 PathfinderSystem::find_path 计算绕墙最短路径
//
//  新增 AI 类型：新建行为组件 + 新增 update_xxx 函数，现有代码零改动。
//
//  流水线位置：MovementSystem 之前（先算速度，再移动）
// =====================================================================

struct Transform;
struct Velocity;
struct EnemyAI;
struct StateMachineAI;
struct ObstacleContact;
struct Health;

class EnemyAISystem : public System {
public:
    void update(World& world, float dt) override;

private:
    // 状态机行为（需 StateMachineAI 组件）
    void update_state_machine(World& world, float dt, Entity player);
    // 随机巡逻行为（需 RandomPatrolAI 组件）
    void update_random_patrol(World& world, float dt);

    // ===== 状态机拆分：集中转换 + 行为分离 =====
    // 集中决定状态转换（含 entry 动作）：只改 sm->state 与 entry 字段，不碰 Velocity
    void decide_state_transition(StateMachineAI* sm, const EnemyAI* ai, const Health* hp,
                                 const Transform* pt, float dist, float dir_x, float dir_y);
    // 各状态行为：只负责算速度，不再改 state
    void update_patrol_behavior(float dt, Transform* t, Velocity* v, EnemyAI* ai,
                                StateMachineAI* sm, ObstacleContact* oc, bool blocked_consumed);
    void update_chase_behavior(float dt, Transform* t, Velocity* v, StateMachineAI* sm,
                               const Transform* pt, float dir_x, float dir_y,
                               ObstacleContact* oc, bool blocked_consumed);
    void update_flee_behavior(Velocity* v, float dir_x, float dir_y);
};
