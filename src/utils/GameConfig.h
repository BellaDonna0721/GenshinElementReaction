#pragma once

// =====================================================================
//  GameConfig —— 世界尺寸等全局常量
// =====================================================================

namespace GameConfig {
    constexpr float WORLD_W      = 1000.0f;  // 世界宽度（画布宽，原点在左下角）
    constexpr float WORLD_H      = 700.0f;   // 世界高度（画布高）

    // === 调试可视化开关 ===
    // 开发测试时设为 true 显示 A* 路径黄线 + 热力图，发布版本设为 false 关闭
    inline constexpr bool DEBUG_PATH_VIS = false;

    // === 路径网格参数 ===
    constexpr float PATH_CELL_SIZE = 25.0f;
    constexpr int   PATH_GRID_W   = 40;      // 1000 / 25
    constexpr int   PATH_GRID_H   = 28;      // 700 / 25

    // 寻路 agent（Boss）的碰撞半径：路径中心线到任一墙体需保持该净空，
    // 由 PathfinderSystem 在网格构建阶段做几何膨胀保证，运行时不再靠碰撞兜底。
    constexpr float PATH_AGENT_CLEARANCE = 36.0f;

    // 归一化坐标（0~1）→ 世界像素坐标
    inline float to_world_x(float nx) { return nx * WORLD_W; }
    inline float to_world_y(float ny) { return ny * WORLD_H; }
}
