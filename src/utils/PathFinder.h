#pragma once
#include "../gameplay/LevelConfig.h"
#include "GameConfig.h"
#include <vector>
#include <algorithm>
#include <cmath>

// =====================================================================
//  PathFinder —— A* 路径规划（独立于 ECS，按需调用）
//
//  核心设计：
//    · 几何膨胀建网格：cell 中心到任一墙体 AABB 的距离 < PATH_AGENT_CLEARANCE
//      即标记不可通行。等价于把墙体按 agent 半径做 Minkowski 膨胀后再栅格化，
//      路径中心线恒有 radius 净空，直线段可贴墙且不穿模，运行时无需碰撞兜底。
//    · 对角规则：No corner cut（两侧正交都可通行才允许）。
//    · 路径简化：LOS 拉直共线/可直达的中间点，保留方向变化的拐角。
//      简化在膨胀后的网格上做，拉直后的线段仍满足净空约束。
//    · 起点/终点若落在不可通行格，取周围 8 邻域欧式最近的可通行格，
//      避免重算时航点指到身后造成前进-回拉抖动。
//
//  不继承 System：不参与每帧调度，只在需要时被调用。
// =====================================================================

namespace PathFinder {

// === 网格 ===
// true = 可通行，false = 墙 / 边界
inline std::vector<std::vector<bool>> grid;

inline void ensure_size() {
    grid.resize(GameConfig::PATH_GRID_H);
    for (auto& row : grid) row.assign(GameConfig::PATH_GRID_W, false);
}

// 世界坐标 → cell 坐标
inline int world_to_cell(float v) {
    int c = static_cast<int>(v / GameConfig::PATH_CELL_SIZE);
    if (c < 0) c = 0;
    if (c >= GameConfig::PATH_GRID_W) c = GameConfig::PATH_GRID_W - 1;
    return c;
}

// cell 坐标 → 该 cell 中心的世界像素坐标（x/y 同公式）
inline float cell_center(int c) {
    return c * GameConfig::PATH_CELL_SIZE + GameConfig::PATH_CELL_SIZE * 0.5f;
}

// 世界坐标线段是否全程落在可通行格上（按半 cell 步长采样）。
// allow_blocked_start=true 时，允许线段起点本身落在膨胀格（实体贴墙卡住时这是常态）——
//   仅跳过起点附近的连续不可走采样，一旦进入可走区即要求后续全程可走。
//   这样既能从卡住位置找到可达格，又不会选到墙对面（中间会被墙挡住）。
inline bool segment_walkable(float x0, float y0, float x1, float y1,
                             bool allow_blocked_start = false) {
    const float step = GameConfig::PATH_CELL_SIZE * 0.5f;
    float dx = x1 - x0, dy = y1 - y0;
    float len = std::sqrt(dx * dx + dy * dy);
    int n = std::max(1, static_cast<int>(len / step));
    bool enforcing = !allow_blocked_start;  // 是否已进入可走区（开始严格校验）
    for (int i = 0; i <= n; i++) {
        float t = static_cast<float>(i) / static_cast<float>(n);
        int cx = world_to_cell(x0 + dx * t);
        int cy = world_to_cell(y0 + dy * t);
        if (cy < 0 || cx < 0 || cy >= GameConfig::PATH_GRID_H || cx >= GameConfig::PATH_GRID_W) {
            if (!enforcing) continue;
            return false;
        }
        if (!grid[cy][cx]) {
            if (!enforcing) continue;  // 仍在起点附近的不可走区，跳过
            return false;
        }
        enforcing = true;
    }
    return enforcing;  // 全程不可走（没进入过可走区）视为失败
}

// 点到轴对齐矩形（中心 rx,ry，半宽 hw，半高 hh）的最近距离平方
inline float point_rect_dist_sq(float px, float py, float rx, float ry, float hw, float hh) {
    float cx = px < rx - hw ? rx - hw : (px > rx + hw ? rx + hw : px);
    float cy = py < ry - hh ? ry - hh : (py > ry + hh ? ry + hh : py);
    float dx = px - cx;
    float dy = py - cy;
    return dx * dx + dy * dy;
}

// 构建 grid：按 agent 净空做几何膨胀
//   cell 中心到任一墙体 AABB 的距离 < clearance → 标记不可通行。
//   这等价于把墙体按 clearance 做 Minkowski 膨胀后再栅格化，中心线恒有净空。
inline void build_grid(const std::vector<WallDef>& walls,
                       float clearance = GameConfig::PATH_AGENT_CLEARANCE) {
    ensure_size();
    const int W = GameConfig::PATH_GRID_W;
    const int H = GameConfig::PATH_GRID_H;
    const float half = GameConfig::PATH_CELL_SIZE * 0.5f;
    const float clear_sq = clearance * clearance;

    // 初始全 true（可通行）
    for (auto& row : grid) std::fill(row.begin(), row.end(), true);

    // 几何膨胀：逐 cell 中心判定净空
    for (int cy = 0; cy < H; cy++) {
        for (int cx = 0; cx < W; cx++) {
            float wx = cx * GameConfig::PATH_CELL_SIZE + half;
            float wy = cy * GameConfig::PATH_CELL_SIZE + half;
            for (const auto& wd : walls) {
                if (point_rect_dist_sq(wx, wy, wd.x, wd.y, wd.w * 0.5f, wd.h * 0.5f) < clear_sq) {
                    grid[cy][cx] = false;
                    break;
                }
            }
        }
    }

    // 边界一圈标记 false（MovementSystem 已按半径 clamp，这里保证中心线不贴边）
    for (int cx = 0; cx < W; cx++) {
        grid[0][cx] = false;
        grid[H - 1][cx] = false;
    }
    for (int cy = 0; cy < H; cy++) {
        grid[cy][0] = false;
        grid[cy][W - 1] = false;
    }
}

// 从世界坐标 (px,py) 出发，在 (cx,cy) 周围找最近的、可达的可通行 cell。
// 可达 = 从 (px,py) 到该 cell 中心的线段可走（allow_blocked_start：容忍 px,py 自身在膨胀格）。
// 关键：只按距离选最近格会选到墙对面的格，导致首航点指向墙里、实体反复撞墙卡死；
//      加线段可达约束后起点一定是实体当前能直接走到的位置。
inline bool find_reachable_cell(float px, float py, int& cx, int& cy, int max_radius = 8) {
    const int W = GameConfig::PATH_GRID_W;
    const int H = GameConfig::PATH_GRID_H;

    int best_cx = -1, best_cy = -1;
    float best_d2 = 1e18f;
    for (int dy = -max_radius; dy <= max_radius; dy++) {
        for (int dx = -max_radius; dx <= max_radius; dx++) {
            int nx = cx + dx, ny = cy + dy;
            if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
            if (!grid[ny][nx]) continue;
            if (!segment_walkable(px, py, cell_center(nx), cell_center(ny), true)) continue;
            float ddx = px - cell_center(nx);
            float ddy = py - cell_center(ny);
            float d2 = ddx * ddx + ddy * ddy;
            if (d2 < best_d2) { best_d2 = d2; best_cx = nx; best_cy = ny; }
        }
    }
    if (best_cx < 0) return false;
    cx = best_cx;
    cy = best_cy;
    return true;
}

// === Bresenham 视线检测（检查 walkability）===
inline bool has_line_of_sight(int x0, int y0, int x1, int y1) {
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    int cx = x0, cy = y0;
    while (true) {
        if (!grid[cy][cx]) return false;
        if (cx == x1 && cy == y1) return true;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; cx += sx; }
        if (e2 <  dx) { err += dx; cy += sy; }
    }
}

// === A* ===
// 输入：起点/终点世界像素坐标，输出：cell 坐标路径（不含起点，已简化）
inline bool find_path(float sx, float sy, float ex, float ey,
                      std::vector<std::pair<short, short>>& path_out) {
    path_out.clear();

    int scx = world_to_cell(sx);
    int scy = world_to_cell(sy);
    int ecx = world_to_cell(ex);
    int ecy = world_to_cell(ey);

    // 起点/终点落在不可通行格（被墙/膨胀区覆盖）：取从当前位置可达的最近可通行格。
    // 必须 LOS 可达，否则首航点会指向墙对面，实体反复撞墙卡死。
    if (!find_reachable_cell(sx, sy, scx, scy)) return false;
    if (!find_reachable_cell(ex, ey, ecx, ecy)) return false;

    if (scx == ecx && scy == ecy) return true;

    const int W = GameConfig::PATH_GRID_W;
    const int H = GameConfig::PATH_GRID_H;
    const int N = W * H;

    std::vector<float> g(N, 1e9f);
    std::vector<short> parent(N, -1);
    g[scy * W + scx] = 0.0f;

    // open set：min-heap，(f_score, index)
    std::vector<std::pair<float, int>> open;
    open.reserve(256);
    int ddx = std::abs(ecx - scx), ddy = std::abs(ecy - scy);
    float h0 = std::max(ddx, ddy) + (1.41421356f - 1.0f) * std::min(ddx, ddy);
    open.push_back({h0, scy * W + scx});
    std::push_heap(open.begin(), open.end(), [](const auto& a, const auto& b) { return a.first > b.first; });

    std::vector<bool> closed(N, false);

    // 8 方向
    const int dx[8]   = { 0, 1, 0, -1,  1, -1,  1, -1};
    const int dy[8]   = {-1, 0, 1,  0,  1,  1, -1, -1};
    const float cost[8] = {1.0f, 1.0f, 1.0f, 1.0f, 1.41421356f, 1.41421356f, 1.41421356f, 1.41421356f};

    bool found = false;
    while (!open.empty()) {
        std::pop_heap(open.begin(), open.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        auto [f_u, u] = open.back();
        open.pop_back();

        if (closed[u]) continue;
        closed[u] = true;

        int ux = u % W, uy = u / W;
        if (ux == ecx && uy == ecy) { found = true; break; }

        float g_u = g[u];
        for (int d = 0; d < 8; d++) {
            int vx = ux + dx[d], vy = uy + dy[d];
            if (vx < 0 || vy < 0 || vx >= W || vy >= H) continue;
            if (!grid[vy][vx]) continue;

            // No corner cut：对角移动两侧正交都可通行才允许
            if (dx[d] != 0 && dy[d] != 0) {
                if (!grid[uy][vx] || !grid[vy][ux]) continue;
            }

            int v = vy * W + vx;
            if (closed[v]) continue;

            float tentative = g_u + cost[d];  // 纯几何代价，无 costmap
            if (tentative < g[v]) {
                g[v] = tentative;
                parent[v] = static_cast<short>(u);
                int ndx = std::abs(ecx - vx), ndy = std::abs(ecy - vy);
                float h = std::max(ndx, ndy) + (1.41421356f - 1.0f) * std::min(ndx, ndy);
                open.push_back({tentative + h, v});
                std::push_heap(open.begin(), open.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
            }
        }
    }

    if (!found) return false;

    // 回溯路径
    std::vector<std::pair<short, short>> path_rev;
    int cur = ecy * W + ecx;
    while (cur != -1) {
        path_rev.push_back({static_cast<short>(cur % W), static_cast<short>(cur / W)});
        cur = parent[cur];
    }
    if (path_rev.empty() || path_rev.back().first != scx || path_rev.back().second != scy)
        path_rev.push_back({static_cast<short>(scx), static_cast<short>(scy)});
    std::reverse(path_rev.begin(), path_rev.end());

    // === 路径简化：视线检测 + 方向一致性 ===
    // 规则：能直线看到 AND 方向没变 → 跳过中间点
    // 方向变了（拐角）哪怕能看到也保留
    std::vector<std::pair<short, short>> simplified;
    simplified.push_back(path_rev[0]);
    int i = 0;
    while (i < static_cast<int>(path_rev.size()) - 1) {
        int j = i + 1;
        while (j + 1 < static_cast<int>(path_rev.size()) &&
               has_line_of_sight(path_rev[i].first, path_rev[i].second,
                                 path_rev[j + 1].first, path_rev[j + 1].second) &&
               (path_rev[j].first - path_rev[i].first) * (path_rev[j + 1].second - path_rev[j].second) ==
               (path_rev[j].second - path_rev[i].second) * (path_rev[j + 1].first - path_rev[j].first)) {
            j++;
        }
        simplified.push_back(path_rev[j]);
        i = j;
    }
    if (simplified.size() > 1) simplified.erase(simplified.begin());

    path_out = std::move(simplified);
    return true;
}

// === 世界坐标转换 + 路径缓存（WASM 导出用）===
struct PathPoint {
    float x, y;
};

inline std::vector<PathPoint> cell_path_to_world(const std::vector<std::pair<short, short>>& cell_path) {
    std::vector<PathPoint> result;
    result.reserve(cell_path.size());
    for (const auto& [cx, cy] : cell_path) {
        result.push_back({
            cx * GameConfig::PATH_CELL_SIZE + GameConfig::PATH_CELL_SIZE * 0.5f,
            cy * GameConfig::PATH_CELL_SIZE + GameConfig::PATH_CELL_SIZE * 0.5f
        });
    }
    return result;
}

}  // namespace PathFinder
