#pragma once
#include <vector>
#include <utility>

// =====================================================================
//  PathFollowAI —— 寻路追击执行器组件（Boss 用）
//
//  只负责"怎么追"：A* 路径缓存 + 平滑转向 + 卡死检测 + 脱困。
//  状态决策（何时追、何时逃）由 StateMachineAI 集中管理，
//  Patrol 执行由 PatrolAI 负责，三者互不感知。
//
//  依赖 PathFinder::find_path 计算绕墙路线。
//  撞墙反馈统一走 ObstacleContact 组件。
//
//  挂载方：ai_type == StateMachine 的敌人（Boss）
// =====================================================================
struct PathFollowAI {
    // === A* 路径缓存 ===
    std::vector<std::pair<short, short>> path;  // cell 坐标路径（不含起点）
    int   path_index     = 0;                   // 当前走到第几个 waypoint
    float repath_timer   = 0.0f;                // 重算冷却计时（最小间隔见 EnemyAISystem）

    // === 平滑航向（独立于 Velocity，避免 CollisionSystem 投影污染）===
    float heading_x      = 1.0f;
    float heading_y      = 0.0f;

    // === 卡死窗口检测 ===
    float stuck_timer    = 0.0f;                // 持续被挡累计时长
    float stuck_origin_x = 0.0f;                // 进入被挡时的位置
    float stuck_origin_y = 0.0f;

    // === 脱困态（卡死时锁定一个方向持续位移，优先于正常寻路）===
    float escape_x       = 0.0f;                // 锁定的脱困方向（单位向量）
    float escape_y       = 0.0f;
    float escape_timer   = 0.0f;                // 脱困剩余时长，>0 表示处于脱困态
};
