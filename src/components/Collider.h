#pragma once

// 碰撞体半径（与 RenderInfo.radius 解耦）
// 逻辑碰撞半径可以和视觉渲染半径不同（例如视觉大、判定小更宽容玩家）
struct Collider {
    float radius = 0.0f;  //逻辑碰撞半径（像素）
};
