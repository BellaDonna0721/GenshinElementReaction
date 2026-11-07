#pragma once
#include <cstdint>

// =====================================================================
//  敌人 AI 组件集（组件组合区分行为类型，而非继承）
//
//  EnemyAI        —— 公共 AI 数据（所有敌人共享：速度/攻击/得分等）
//  StateMachineAI —— 状态机行为数据（巡逻/警戒/战斗/逃跑 四态）
//  RandomPatrolAI —— 随机巡逻行为数据（无目的漫游，不主动追击）
//
//  设计原则：组件组合优于继承。新增 AI 类型 = 新组件 + EnemyAISystem 新 update 函数，
//  现有代码零改动（开闭原则）。
// =====================================================================

// AI 行为类型（用于 EnemyPreset 配置选择挂载哪个行为组件）
enum class AIType : uint8_t {
    StateMachine = 0,  // 四态状态机（默认）
    RandomPatrol = 1,  // 随机巡逻
};

// =====================================================================
//  EnemyAI —— 公共 AI 数据组件
//  所有敌人共享，与具体行为无关。
//  move_speed 被 MovementSystem 读取；攻击字段被 CollisionSystem 读取。
// =====================================================================
struct EnemyAI {
    uint8_t enemy_type      = 0;       // 敌人类型（存 ElementType 值，血条显示用）
    float   move_speed      = 60.0f;   // 移动速度（像素/秒）
    float   alert_radius    = 180.0f;  // 警戒/探测范围（像素）
    float   attack_radius   = 36.0f;   // 攻击范围（像素，接触伤害）
    float   attack_power    = 10.0f;   // 每次接触伤害
    float   attack_cooldown = 0.6f;    // 攻击间隔（秒）
    float   attack_timer    = 0.0f;    // 当前攻击冷却计时
    int     score_value     = 10;      // 击杀得分
    float   home_x          = 0.0f;    // 出生点 X
    float   home_y          = 0.0f;    // 出生点 Y
};

// =====================================================================
//  StateMachineAI —— 状态机行为数据
//  状态转换与行为执行逻辑由 EnemyAISystem::update_state_machine 处理。
// =====================================================================
enum class AIState : uint8_t {
    Patrol = 0,  // 巡逻：在出生点附近随机游走
    Alert  = 1,  // 警戒：玩家进入警戒范围，停止移动并面向玩家
    Combat = 2,  // 战斗：玩家进入攻击范围，追击并造成接触伤害
    Flee   = 3,  // 逃跑：血量低于阈值，朝玩家反方向逃离
};

struct StateMachineAI {
    AIState state           = AIState::Patrol;  // 当前状态
    float   flee_hp_ratio   = 0.25f;             // 血量比例低于此值进入逃跑
    float   patrol_timer    = 0.0f;              // 巡逻换点计时
    float   patrol_interval = 3.0f;              // 巡逻换点间隔（秒）
    float   patrol_radius   = 100.0f;            // 巡逻半径
    float   target_x        = 0.0f;              // 当前巡逻目标点 X
    float   target_y        = 0.0f;              // 当前巡逻目标点 Y
};

// =====================================================================
//  RandomPatrolAI —— 随机巡逻行为数据
//  敌人在出生点附近随机漫游，不主动追击玩家。
//  接触伤害仍由 CollisionSystem 处理（玩家主动撞上时生效）。
// =====================================================================
struct RandomPatrolAI {
    float   wander_radius   = 100.0f;  // 漫游范围（相对出生点）
    float   change_timer    = 0.0f;    // 换方向计时
    float   change_interval = 2.5f;    // 换方向间隔（秒）
    float   target_x        = 0.0f;    // 当前漫游目标点 X
    float   target_y        = 0.0f;    // 当前漫游目标点 Y
};
