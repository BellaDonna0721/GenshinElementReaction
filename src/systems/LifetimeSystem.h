#pragma once
#include "../core/World.h"

// 生命周期系统：第 2 步流水线（在移动之前先清寿命到的，避免白跑一帧碰撞）
//   遍历所有带 Lifetime 的实体 → remaining -= dt
//   到期后按 IdentityTag 分支：子弹还池 / 其他直接 destroy
class LifetimeSystem : public System {
public:
    void update(World& world, float dt) override;
};
