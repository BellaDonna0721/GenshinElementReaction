#pragma once
#include "../gameplay/LevelConfig.h"
#include "../utils/GameConfig.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <queue>

// =====================================================================
//  PathfinderSystem —— A* 路径规划（独立于 ECS，按需调用）
//
//  核心设计：
//    · 硬膨胀 = 0（不堵通道），改用 Costmap 分级代价场
//    · 离墙越近代价越高，A* 自动绕行；通道窄时代价虽高但可通行
//    · 对角规则：No corner cut（两侧正交都可通行才允许）
//    · 结果自动做视线简化，消除锯齿 waypoint
//
//  不继承 System：不参与每帧调度，只在需要时被调用。
// =====================================================================

namespace PathfinderSystem {

// === 网格 + 代价场 ===
// grid: 可通行 true / 阻挡 false
// costmap: 进入该 cell 的额外代价（离墙越近越高）
inline std::vector<std::vector<bool>> grid;
inline std::vector<std::vector<float>> costmap;

// Costmap 参数：离墙距离 → 额外代价
// 目的：A* 尽量远离墙走中间，通道窄时也能通过（代价高但不堵）
static constexpr int   COSTMAP_RADIUS = 4;   // 扫描半径（cell）
static constexpr float COSTMAP_CLOSE  = 64.0f; // 距墙 1 cell
static constexpr float COSTMAP_NEAR   = 4.0f; // 距墙 2 cell   
static constexpr float COSTMAP_MID    = 2.0f; // 距墙 3 cell
static constexpr float COSTMAP_FAR    = 0.0f; // 距墙 4 cell
// ≥ 4 cell: 0.0（无惩罚）

inline void ensure_size() {
    grid.resize(GameConfig::PATH_GRID_H);
    for (auto& row : grid) row.assign(GameConfig::PATH_GRID_W, false);
    costmap.resize(GameConfig::PATH_GRID_H);
    for (auto& row : costmap) row.assign(GameConfig::PATH_GRID_W, 0.0f);
}

// 世界坐标 → cell 坐标
inline int world_to_cell(float v) {
    int c = static_cast<int>(v / GameConfig::PATH_CELL_SIZE);
    if (c < 0) c = 0;
    if (c >= GameConfig::PATH_GRID_W) c = GameConfig::PATH_GRID_W - 1;
    return c;
}

// 构建 grid + costmap：墙本身为 blocked，周围代价场分级衰减
inline void build_grid(const std::vector<WallDef>& walls) {
    ensure_size();
    const int W = GameConfig::PATH_GRID_W;
    const int H = GameConfig::PATH_GRID_H;

    // 第一步：墙覆盖 cell 标记 blocked
    for (auto& row : grid) std::fill(row.begin(), row.end(), true);
    for (const auto& wd : walls) {
        float x0 = wd.x - wd.w / 2.0f;
        float x1 = wd.x + wd.w / 2.0f;
        float y0 = wd.y - wd.h / 2.0f;
        float y1 = wd.y + wd.h / 2.0f;

        int cx0 = world_to_cell(x0);
        int cx1 = world_to_cell(x1);
        int cy0 = world_to_cell(y0);
        int cy1 = world_to_cell(y1);

        for (int cy = cy0; cy <= cy1 && cy < H; cy++)
            for (int cx = cx0; cx <= cx1 && cx < W; cx++)
                if (cx >= 0 && cy >= 0)
                    grid[cy][cx] = false;
    }

    // 边界一圈标记 false
    for (int cx = 0; cx < W; cx++) {
        grid[0][cx] = false;
        grid[H - 1][cx] = false;
    }
    for (int cy = 0; cy < H; cy++) {
        grid[cy][0] = false;
        grid[cy][W - 1] = false;
    }

    // 第二步：BFS 计算每个 cell 到最近墙的曼哈顿距离，映射成分级代价
    // 多源 BFS：从所有墙 cell 同时出发，一次 O(W*H) 算出所有距离
    std::vector<std::vector<int>> dist(H, std::vector<int>(W, COSTMAP_RADIUS + 1));
    std::queue<std::pair<int,int>> q;
    for (int cy = 0; cy < H; cy++) {
        for (int cx = 0; cx < W; cx++) {
            if (!grid[cy][cx]) { dist[cy][cx] = 0; q.push({cx, cy}); }
        }
    }

    // 4 方向 BFS（正交），算的是曼哈顿距离
    const int bdx[4] = {0, 1, 0, -1};
    const int bdy[4] = {-1, 0, 1, 0};
    while (!q.empty()) {
        auto [cx, cy] = q.front(); q.pop();
        int cd = dist[cy][cx];
        if (cd >= COSTMAP_RADIUS) continue;  // 足够远了，不需要再扩散
        for (int d = 0; d < 4; d++) {
            int nx = cx + bdx[d], ny = cy + bdy[d];
            if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
            if (dist[ny][nx] > cd + 1) {
                dist[ny][nx] = cd + 1;
                q.push({nx, ny});
            }
        }
    }

    // 距离 → 代价映射
    for (int cy = 0; cy < H; cy++) {
        for (int cx = 0; cx < W; cx++) {
            if (!grid[cy][cx]) { costmap[cy][cx] = 1e9f; continue; }
            int d = dist[cy][cx];
            if      (d == 0) costmap[cy][cx] = 1e9f;  // 不应出现（grid==false 已处理）
            else if (d == 1) costmap[cy][cx] = COSTMAP_CLOSE;
            else if (d == 2) costmap[cy][cx] = COSTMAP_NEAR;
            else if (d == 3) costmap[cy][cx] = COSTMAP_MID;
            else if (d == 4) costmap[cy][cx] = COSTMAP_FAR;
            else             costmap[cy][cx] = 0.0f;
        }
    }
}

// === Bresenham 视线检测（检查 walkability，不含代价） ===
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

// === A*（Costmap 版） ===
// 输入：起点/终点世界像素坐标，输出：cell 坐标路径（不含起点，已简化）
inline bool find_path(float sx, float sy, float ex, float ey,
                      std::vector<std::pair<short, short>>& path_out) {
    path_out.clear();

    int scx = world_to_cell(sx);
    int scy = world_to_cell(sy);
    int ecx = world_to_cell(ex);
    int ecy = world_to_cell(ey);

    // 起点 blocked：尝试相邻
    if (!grid[scy][scx]) {
        if (scx + 1 < GameConfig::PATH_GRID_W && grid[scy][scx + 1]) scx++;
        else if (scy + 1 < GameConfig::PATH_GRID_H && grid[scy + 1][scx]) scy++;
        else return false;
    }
    if (!grid[ecy][ecx]) {
        if (ecx + 1 < GameConfig::PATH_GRID_W && grid[ecy][ecx + 1]) ecx++;
        else if (ecy + 1 < GameConfig::PATH_GRID_H && grid[ecy + 1][ecx]) ecy++;
        else return false;
    }

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

            // step cost = 基础移动代价 + 进入目标 cell 的代价场惩罚
            float step = cost[d] + costmap[vy][vx];
            float tentative = g_u + step;
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

    // === 路径简化：视线检测消除冗余 waypoint ===
    std::vector<std::pair<short, short>> simplified;
    simplified.push_back(path_rev[0]);
    int i = 0;
    while (i < static_cast<int>(path_rev.size()) - 1) {
        int j = i + 1;
        while (j + 1 < static_cast<int>(path_rev.size()) &&
               has_line_of_sight(path_rev[i].first, path_rev[i].second,
                                 path_rev[j + 1].first, path_rev[j + 1].second)) {
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

// 将 cell 坐标路径转为世界坐标 waypoint（cell 中心）
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

}  // namespace PathfinderSystem
