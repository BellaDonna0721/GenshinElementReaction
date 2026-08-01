#include "MovementSystem.h"
#include "../components/Transform.h"
#include "../components/Velocity.h"
#include "../components/IdentityTag.h"
#include "InputSystem.h"
#include <cmath>

void MovementSystem::update(World& world, float dt) {
    const float PLAYER_SPEED   = 200.0f;
    const float BULLET_SPEED   = 520.0f;
    const float FRICTION       = 0.85f;
    const float WORLD_W        = 800.0f;
    const float WORLD_H        = 600.0f;

    std::vector<Entity> out_of_bounds;

    // 统一处理所有带 Transform + Velocity 的实体
    //   - Identity::Bullet：BULLET_SPEED 移动 + 出界回收，不摩擦/不夹紧
    //   - 其他（玩家/敌人）：PLAYER_SPEED 移动 + 边界夹紧 + 摩擦衰减
    for (auto e : world.query<Transform, Velocity>()) {
        auto* t = world.get_component<Transform>(e);
        auto* v = world.get_component<Velocity>(e);

        bool is_bullet = false;
        auto* id_tag = world.get_component<IdentityTag>(e);
        if (id_tag && id_tag->identity_type == Identity::Bullet) is_bullet = true;

        float speed = is_bullet ? BULLET_SPEED : PLAYER_SPEED;

        t->x += v->vx * speed * dt;
        t->y += v->vy * speed * dt;

        if (is_bullet) {
            // 子弹：边界外 20 像素就回收（不是夹紧，直接销毁）
            if (t->x < -20.0f || t->x > WORLD_W + 20.0f ||
                t->y < -20.0f || t->y > WORLD_H + 20.0f)
            {
                out_of_bounds.push_back(e);
            }
            // 子弹不做摩擦
        } else {
            // 玩家/敌人：边界夹紧（留 20 像素边距）
            if (t->x < 20.0f)                 t->x = 20.0f;
            if (t->x > WORLD_W - 20.0f)       t->x = WORLD_W - 20.0f;
            if (t->y < 20.0f)                 t->y = 20.0f;
            if (t->y > WORLD_H - 20.0f)       t->y = WORLD_H - 20.0f;

            // 摩擦衰减（松开按键后慢慢停下）
            v->vx *= FRICTION;
            v->vy *= FRICTION;
            if (std::abs(v->vx) < 0.01f) v->vx = 0.0f;
            if (std::abs(v->vy) < 0.01f) v->vy = 0.0f;
        }
    }

    // 越界子弹：归还到对象池
    for (auto e : out_of_bounds) {
        release_bullet_entity(world, e);
    }
}
