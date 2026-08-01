#pragma once
#include "../core/World.h"

// 移动系统：第 3 步流水线（在碰撞检测之前先算本帧新位置）
//   遍历所有 Transform+Velocity → 按 IdentityTag 分支：
//     - Bullet：SPEED=520，飞出世界边界立即归还池
//     - 玩家/敌人：SPEED=200，边界夹紧(不出屏幕)+ 摩擦衰减(vx*=0.85)
class MovementSystem : public System {
public:
    void update(World& world, float dt) override;
};
