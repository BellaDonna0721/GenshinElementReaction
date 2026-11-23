#pragma once

// =====================================================================
//  PatrolAI —— 通用巡逻行为数据
//
//  所有会"巡逻"的敌人共享（小怪随机巡逻 / Boss 状态机的 Patrol 态）。
//  行为逻辑由 EnemyAISystem::update_patrol_behavior 统一实现：
//    在出生点附近随机选点游走，撞墙时沿推开方向反向避让。
//
//  挂载方：
//    · 小怪（ai_type == RandomPatrol）：巡逻是全部行为
//    · Boss（ai_type == StateMachine）：Patrol 态消费本组件，
//      状态切换由 StateMachineAI 管理，两者互不感知
// =====================================================================
struct PatrolAI {
    // —— 配置段（真源：EnemyPreset，spawn 时填充；运行时只读）——
    float   patrol_interval = 2.5f;    // 换点间隔（秒）
    float   patrol_radius   = 100.0f;  // 巡逻半径（相对出生点）

    // —— 运行时状态段 ——
    float   patrol_timer    = 0.0f;    // 换点计时
    float   target_x        = 0.0f;   // 当前巡逻目标点 X
    float   target_y        = 0.0f;   // 当前巡逻目标点 Y
};
