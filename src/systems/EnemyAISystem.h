#pragma once
#include "../systems/System.h"
#include "../core/Entity.h"

// =====================================================================
//  EnemyAISystem —— 敌人 AI 系统
//
//  职责：根据挂载的行为组件分派不同 AI 逻辑
//    · update_state_machine —— 三态状态机（巡逻/追击/逃跑，Boss 专用）
//    · update_patrol —— 随机巡逻（无目的漫游，不主动追击，普通小怪）
//
//  巡逻数据统一在 PatrolAI 组件（Boss 的 Patrol 态与小怪共用
//  update_patrol_behavior 实现）。
//  Boss 追击依赖 PathFinder::find_path 计算绕墙最短路径
//
//  新增 AI 类型：新建行为组件 + 新增 update_xxx 函数，现有代码零改动。
//
//  流水线位置：MovementSystem 之前（先算速度，再移动）
// =====================================================================

struct Transform;
struct Velocity;
struct EnemyAI;
struct StateMachineAI;
struct PatrolAI;
struct PathFollowAI;
struct ObstacleContact;
struct Health;

class EnemyAISystem : public System {
public:
    void update(World& world, float dt) override;

private:
    // 状态机行为（需 StateMachineAI + PathFollowAI）
    void update_state_machine(World& world, float dt, Entity player);
    // 随机巡逻行为（需 PatrolAI 组件且 ai_type == RandomPatrol，普通小怪）
    void update_patrol(World& world, float dt);

    // ===== 状态机拆分：集中转换 + 行为分离 =====
    // 集中决定状态转换（含 entry 动作）：只改 sm->state 与 pf/pc 残留清理，不碰 Velocity
    void decide_state_transition(StateMachineAI* sm, const EnemyAI* ai, PatrolAI* pc,
                                 PathFollowAI* pf, const Health* hp, const Transform* pt,
                                 float dist, float dir_x, float dir_y);
    // 各状态行为：只负责算速度，不再改 state
    // 巡逻核心（Boss Patrol 态与小怪随机巡逻共用，数据在 PatrolAI 组件）
    void update_patrol_behavior(float dt, Transform* t, Velocity* v, EnemyAI* ai,
                                PatrolAI* pc, ObstacleContact* oc);
    // 追击核心（Boss Chase 态用，数据在 PathFollowAI 组件）
    void update_chase_behavior(float dt, Transform* t, Velocity* v, PathFollowAI* pf,
                               const Transform* pt, float dir_x, float dir_y,
                               ObstacleContact* oc);
    void update_flee_behavior(Velocity* v, float dir_x, float dir_y);
};
