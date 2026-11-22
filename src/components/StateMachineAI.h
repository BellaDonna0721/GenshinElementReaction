#pragma once
#include <cstdint>
#include <vector>
#include <utility>

// =====================================================================
//  StateMachineAI —— 状态机行为数据（Boss 用）
//  状态：Patrol / Chase / Flee
//  Chase 态依赖 PathfinderSystem::find_path 计算绕墙路线
// =====================================================================
enum class AIState : uint8_t {
    Patrol = 0,  // 巡逻：在出生点附近随机游走
    Chase  = 1,  // 追击：A* 路径追玩家，遇墙自动绕路
    Flee   = 2,  // 逃跑：血量低于阈值，朝玩家反方向逃离
};

struct StateMachineAI {
    // —— 运行时状态段 ——
    AIState state           = AIState::Patrol;  // 当前状态

    // —— 配置段（真源：EnemyPreset，仅 ai_type==StateMachine 时填充；运行时只读）——
    float   flee_hp_ratio   = 0.25f;             // 血量比例低于此值进入逃跑
    float   patrol_interval = 3.0f;              // 巡逻换点间隔（秒）
    float   patrol_radius   = 100.0f;            // 巡逻半径

    // —— 运行时状态段 ——
    float   patrol_timer    = 0.0f;              // 巡逻换点计时
    float   target_x        = 0.0f;              // 当前巡逻目标点 X
    float   target_y        = 0.0f;              // 当前巡逻目标点 Y

    // === A* 路径缓存（Chase 态消费） ===
    std::vector<std::pair<short, short>> path;     // cell 坐标路径（不含起点）
    int   path_index      = 0;                      // 当前走到第几个 waypoint
    float repath_timer    = 0.0f;                   // 重算冷却计时（最小间隔见 EnemyAISystem）
    short last_target_cx  = -1;                     // 上次目标（玩家）所在 cell x
    short last_target_cy  = -1;

    // 撞墙反馈统一走 ObstacleContact 组件（ObstacleContact.h）

    // === 路径跟随（Chase 态消费：平滑转向 + 卡死检测）===
    float heading_x   = 1.0f;   // 平滑后的移动方向（单位向量，独立于 Velocity 避免碰撞投影污染）
    float heading_y   = 0.0f;
    float stuck_timer = 0.0f;   // 持续被挡的累计时长（窗口位移检测用）
    float stuck_origin_x = 0.0f;// 进入被挡时的位置，用于判定窗口内是否真的没动
    float stuck_origin_y = 0.0f;

    // === 脱困态（Chase 态：卡死时锁定一个方向持续位移，优先于正常寻路）===
    float escape_x    = 0.0f;   // 锁定的脱困方向（单位向量）
    float escape_y    = 0.0f;
    float escape_timer = 0.0f;  // 脱困剩余时长，>0 表示处于脱困态
};
