#pragma once
#include "../core/World.h"

// 移动系统：第 3 步流水线（在碰撞检测之前先算本帧新位置）
//   遍历所有 Transform+Velocity → 通用位移：方向(vx,vy) × 大小(speed)
//   速度完全数据驱动（Velocity.speed），不再按身份硬编码；
//   冻结定身由 EnemyAISystem 在算速度前把 vx/vy 归零，此处不查 ElementStatus。
//   位移后按身份做边界约束：子弹出界回收，玩家/敌人夹紧不出屏幕。
class MovementSystem : public System {
public:
    void update(World& world, float dt) override;
};
