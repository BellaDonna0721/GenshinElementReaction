#pragma once
#include <cstdint>

// =====================================================================
//  EnemyAI —— 敌人 AI 状态机数据组件
//
//  单一职责：仅存储状态机运行时所需的数据。
//  状态转换与行为执行逻辑由 EnemyAISystem 统一处理。
//
//  状态机四态：
//    Patrol  — 巡逻：在出生点附近随机游走
//    Alert   — 警戒：玩家进入警戒范围，停止移动并面向玩家
//    Combat  — 战斗：玩家进入攻击范围，追击并造成接触伤害
//    Flee    — 逃跑：血量低于阈值，朝玩家反方向逃离
//
//  参数全部来自 EnemyPreset，不同敌人差异化配置。
// =====================================================================

enum class AIState : uint8_t {
    Patrol = 0,     //巡逻
    Alert  = 1,     //警戒
    Combat = 2,     //战斗
    Flee   = 3,     //逃跑
};

struct EnemyAI {
    AIState state          = AIState::Patrol;  // 当前状态
    float   alert_radius   = 180.0f;           // 警戒范围（像素）
    float   attack_radius  = 36.0f;            // 攻击范围（像素，接触伤害）
    float   attack_power   = 10.0f;            // 每次接触伤害
    float   attack_cooldown = 0.6f;            // 攻击间隔（秒）
    float   attack_timer   = 0.0f;             // 当前攻击冷却计时
    float   flee_hp_ratio  = 0.25f;            // 血量比例低于此值进入逃跑
    float   move_speed     = 60.0f;            // 移动速度（像素/秒）
    float   patrol_timer   = 0.0f;             // 巡逻换点计时
    float   patrol_interval = 3.0f;            // 巡逻换点间隔（秒）
    float   home_x         = 0.0f;             // 出生点 X（巡逻范围中心）
    float   home_y         = 0.0f;             // 出生点 Y
    float   patrol_radius  = 100.0f;           // 巡逻半径
    float   target_x       = 0.0f;             // 当前巡逻目标点 X
    float   target_y       = 0.0f;             // 当前巡逻目标点 Y
    int     score_value    = 10;               // 击杀得分（来自 EnemyPreset）
};
