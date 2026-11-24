#pragma once
#include <cstdint>

// =====================================================================
//  StateMachineAI —— 状态机决策壳（Boss 用）
//
//  只负责"现在做什么"：Patrol / Chase / Flee 三态调度。
//  执行能力拆到独立组件：
//    · 巡逻 → PatrolAI（与小怪共用）
//    · 追击 → PathFollowAI（A* 寻路 + 卡死脱困）
//    · 逃跑 → 无组件（朝玩家反方向，speed_mult 设 1.2）
//
//  这样新增"巡逻但不会寻路"的中型怪，只需不挂 PathFollowAI，
//  现有代码零改动。
// =====================================================================
enum class AIState : uint8_t {
    Patrol = 0,  // 巡逻：PatrolAI 消费
    Chase  = 1,  // 追击：PathFollowAI 消费
    Flee   = 2,  // 逃跑：纯函数（朝玩家反方向）
};

struct StateMachineAI {
    AIState state         = AIState::Patrol;  // 当前状态

    // —— 配置段（真源：EnemyPreset；运行时只读）——
    float   flee_hp_ratio = 0.25f;            // 血量比例低于此值进入逃跑
};
