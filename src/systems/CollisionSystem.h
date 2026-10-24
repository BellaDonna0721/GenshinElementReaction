#pragma once
#include "../core/World.h"

// 碰撞系统：第 4 步流水线
//   处理三类碰撞：
//     ① 玩家/敌人（球 SphereCollider） vs 墙体（矩 BoxCollider）→ 沿法线推开
//     ② 子弹（球） vs 敌人（球）→ 命中写 pending_element/pending_gauge + 子弹归池
//     ③ 子弹（球） vs 墙体（矩）→ 子弹撞墙销毁归池
class CollisionSystem : public System {
public:
    void update(World& world, float dt) override;
};
