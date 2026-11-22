#pragma once

// =====================================================================
//  RandomPatrolAI —— 随机巡逻行为数据
//  敌人在出生点附近随机漫游，不主动追击玩家。
//  接触伤害仍由 CollisionSystem 处理（玩家主动撞上时生效）。
// =====================================================================
struct RandomPatrolAI {
    // —— 配置段（真源：EnemyPreset，仅 ai_type==RandomPatrol 时填充；运行时只读）——
    float   wander_radius   = 100.0f;  // 漫游范围（相对出生点）
    float   change_interval = 2.5f;    // 换方向间隔（秒）

    // —— 运行时状态段 ——
    float   change_timer    = 0.0f;    // 换方向计时
    float   target_x        = 0.0f;    // 当前漫游目标点 X
    float   target_y        = 0.0f;    // 当前漫游目标点 Y

    // 撞墙反馈统一走 ObstacleContact 组件（ObstacleContact.h）
};
