#pragma once
#include "../core/World.h"

// 碰撞系统：流水线第 6 步
//   职责：纯几何碰撞检测 + 物理响应（推开/沿墙滑动/敌人互推）+ 碰撞→AI 反馈
//   命中后只写 HitEventQueue（碰撞事实的唯一出口），
//   附着/特效/归池等业务由后续系统（ElementReactionSystem / OutputSystem）消费
class CollisionSystem : public System {
public:
    void update(World& world, float dt) override;
};
