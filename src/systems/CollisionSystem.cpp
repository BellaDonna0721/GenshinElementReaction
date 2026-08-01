#include "CollisionSystem.h"
#include "../components/Transform.h"
#include "../components/Collider.h"
#include "../components/IdentityTag.h"
#include "../components/ElementPayload.h"
#include "../components/ElementStatus.h"
#include "InputSystem.h"

void CollisionSystem::update(World& world, float dt) {
    (void)dt;

    // 先收集子弹层和敌人层（用 IdentityTag 在运行时过滤）
    std::vector<Entity> bullets;
    std::vector<Entity> enemies;

    for (auto e : world.query<Transform, Collider, IdentityTag>()) {
        auto* id = world.get_component<IdentityTag>(e);
        if (id->identity_type == Identity::Bullet)       bullets.push_back(e);
        else if (id->identity_type == Identity::Enemy)   enemies.push_back(e);
    }

    std::vector<Entity> consumed;
    consumed.reserve(bullets.size());

    for (auto b : bullets) {
        auto* bt      = world.get_component<Transform>(b);
        auto* bc      = world.get_component<Collider>(b);
        auto* payload = world.get_component<ElementPayload>(b);
        bool  hit     = false;

        for (auto en : enemies) {
            // enemies 已经用 Identity::Enemy 过滤了，无需再判断类型
            auto* et = world.get_component<Transform>(en);
            auto* ec = world.get_component<Collider>(en);
            float dx = bt->x - et->x;
            float dy = bt->y - et->y;
            // 碰撞半径完全用 Collider.radius 计算（与 RenderInfo.radius 解耦）
            float r  = bc->radius + ec->radius;

            if (dx*dx + dy*dy <= r * r) {
                // 命中：把子弹携带的元素类型 + 强度档写到敌人的 pending 槽
                auto* es = world.get_component<ElementStatus>(en);
                if (payload && es) {
                    es->pending_element  = payload->element;
                    es->pending_strength = payload->strength;
                }
                hit = true;
                break;
            }
        }

        if (hit) consumed.push_back(b);
    }

    // 命中的子弹：归还对象池
    for (auto b : consumed) {
        release_bullet_entity(world, b);
    }
}
