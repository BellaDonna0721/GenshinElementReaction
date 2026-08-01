#pragma once
#include "../core/World.h"

// 碰撞系统：第 4 步流水线
//   分两层 query：子弹层(Identity=Bullet) vs 敌人层(Identity=Enemy)
//   O(N*M) 圆形碰撞 → 命中敌人写 ElementStatus.pending_element/pending_gauge → 命中小子弹立即归还池
class CollisionSystem : public System {
public:
    void update(World& world, float dt) override;
};
