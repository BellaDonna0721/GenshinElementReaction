#include "MovementSystem.h"
#include "../components/Transform.h"
#include "../components/Velocity.h"
#include "../components/IdentityTag.h"
#include "../components/SphereCollider.h"
#include "../gameplay/BulletPool.h"
#include "../utils/GameConfig.h"
#include <vector>

void MovementSystem::update(World& world, float dt) {
    std::vector<Entity> out_of_bounds;

    // 位移：方向(vx,vy 归一化) × 基础速度(speed) × 临时倍率(speed_mult) → Transform
    // 速度完全数据驱动，不再按身份硬编码；
    // 冻结定身已由 EnemyAISystem 在算速度前把 vx/vy 归零，此处无需再查 ElementStatus。
    for (auto e : world.query<Transform, Velocity, IdentityTag>()) {
        auto* t  = world.get_component<Transform>(e);
        auto* v  = world.get_component<Velocity>(e);
        auto* id = world.get_component<IdentityTag>(e);

        t->x += v->vx * v->speed * v->speed_mult * dt;
        t->y += v->vy * v->speed * v->speed_mult * dt;

        // 边界约束：子弹出界回收 / 玩家-敌人夹紧（仅此处因边界策略不同需区分身份）
        if (id->identity_type == Identity::Bullet) {
            constexpr float CULL_PAD = 50.0f;
            if (t->x < -CULL_PAD || t->x > GameConfig::WORLD_W + CULL_PAD ||
                t->y < -CULL_PAD || t->y > GameConfig::WORLD_H + CULL_PAD)
            {
                out_of_bounds.push_back(e);
            }
        } else if (id->identity_type == Identity::Player ||
                   id->identity_type == Identity::Enemy) {
            float pr = 0.0f;
            auto* col = world.get_component<SphereCollider>(e);
            if (col) pr = col->radius;
            if (t->x < pr)                       t->x = pr;
            if (t->x > GameConfig::WORLD_W - pr) t->x = GameConfig::WORLD_W - pr;
            if (t->y < pr)                       t->y = pr;
            if (t->y > GameConfig::WORLD_H - pr) t->y = GameConfig::WORLD_H - pr;
        }
    }

    // 处理出界子弹
    for (auto e : out_of_bounds) {
        release_bullet_entity(world, e);
    }
}
