#pragma once
#include <cstdint>

// =====================================================================
//  敌人 AI 组件集
//
//  EnemyAI        —— 公共 AI 数据（components/EnemyAI.h，所有敌人共享）
//  StateMachineAI —— 状态机决策壳（components/StateMachineAI.h，Boss 用）
//  PatrolAI       —— 通用巡逻行为数据（components/PatrolAI.h，小怪 + Boss Patrol 态共用）
//  PathFollowAI   —— A* 寻路追击执行器（components/PathFollowAI.h，Boss Chase 态用）
//  ObstacleContact—— 障碍接触反馈（components/ObstacleContact.h，通用）
//
//  设计原则：组件组合优于继承。新增 AI 类型 = 新组件头文件 +
//  EnemyAISystem 新 update 函数 + spawn_enemy 新挂载分支，现有代码零改动。
// =====================================================================

// AI 行为类型（用于 EnemyPreset 配置选择挂载哪个行为组件）
enum class AIType : uint8_t {
    StateMachine = 0,  // 三态状态机（巡逻/追击/逃跑）
    RandomPatrol = 1,  // 随机巡逻（不追击玩家）
};

// =====================================================================
//  EnemyAI —— 公共 AI 数据组件
//  所有敌人共享，与具体行为无关。
//
//  字段分三段：
//    · 配置段   —— 真源在 EnemyPreset，spawn_enemy 填充，运行时只读（此处默认值仅兜底）
//    · 运行时状态段 —— 已抽离到独立 Cooldown / ObstacleContact / StateMachineAI / PatrolAI
//    · 实例段   —— spawn 时确定，之后不变
// =====================================================================
struct EnemyAI {
    // —— 配置段（真源：EnemyPreset）——
    AIType   ai_type        = AIType::RandomPatrol;  // AI 行为类型（生成时决定挂载哪个行为组件，运行时只读）
    uint8_t enemy_type      = 0;       // 敌人类型（存 ElementType 值，血条显示用）
    float   move_speed      = 60.0f;   // 移动速度（像素/秒）
    float   alert_radius    = 180.0f;  // 警戒/探测范围（像素）
    float   attack_radius   = 36.0f;   // 攻击范围（像素，接触伤害）
    float   attack_power    = 10.0f;   // 每次接触伤害
    int     score_value     = 10;      // 击杀得分

    // —— 实例初始数据段（spawn 确定，之后不变）——
    float   home_x          = 0.0f;    // 出生点 X
    float   home_y          = 0.0f;    // 出生点 Y
};
