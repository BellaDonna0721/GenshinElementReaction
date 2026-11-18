#pragma once

// =====================================================================
//  GameConfig —— 世界尺寸等全局常量
// =====================================================================

namespace GameConfig {
    constexpr float WORLD_W      = 1000.0f;  // 世界宽度（画布宽，原点在左下角）
    constexpr float WORLD_H      = 700.0f;   // 世界高度（画布高）

    // === 路径网格参数 ===
    constexpr float PATH_CELL_SIZE = 25.0f;
    constexpr int   PATH_GRID_W   = 40;      // 1000 / 25
    constexpr int   PATH_GRID_H   = 28;      // 700 / 25
    // 推开 + Chase 态 blocked 触发重算路径，自然绕开。

    // 归一化坐标（0~1）→ 世界像素坐标
    inline float to_world_x(float nx) { return nx * WORLD_W; }
    inline float to_world_y(float ny) { return ny * WORLD_H; }
}
