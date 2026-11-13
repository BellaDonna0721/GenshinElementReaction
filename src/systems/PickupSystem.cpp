#include "PickupSystem.h"
#include "../core/World.h"
#include "../components/Transform.h"
#include "../components/SphereCollider.h"
#include "../components/IdentityTag.h"
#include "../components/Health.h"
#include "../components/ShieldPickup.h"
#include "../components/singletonComponents/GameState.h"
#include <cmath>
#include <vector>
#include <algorithm>

void PickupSystem::update(World& world, float dt) {
    (void)dt;

    // 定位玩家
    Entity player = INVALID_ENTITY;
    Transform* pt = nullptr;
    SphereCollider* pc = nullptr;
    for (auto e : world.query<Transform, SphereCollider, IdentityTag>()) {
        auto* id = world.get_component<IdentityTag>(e);
        if (id && id->identity_type == Identity::Player) {
            player = e;
            pt = world.get_component<Transform>(e);
            pc = world.get_component<SphereCollider>(e);
            break;
        }
    }
    if (player == INVALID_ENTITY || !pt || !pc) return;

    GameState* gs = world.get_component<GameState>(0);
    if (!gs) return;

    // 收集本帧被拾取的道具（遍历中不销毁，避免迭代器失效）
    std::vector<Entity> picked;

    for (auto e : world.query<Transform, SphereCollider, IdentityTag>()) {
        auto* id = world.get_component<IdentityTag>(e);
        if (!id || id->identity_type != Identity::Pickup) continue;

        auto* t = world.get_component<Transform>(e);
        auto* col = world.get_component<SphereCollider>(e);
        if (!t || !col) continue;

        // 圆-圆碰撞：玩家与道具
        float dx = pt->x - t->x;
        float dy = pt->y - t->y;
        float r  = pc->radius + col->radius;
        if (dx * dx + dy * dy > r * r) continue;

        // —— 根据道具效果组件执行对应效果 ——

        // 护盾晶片
        if (auto* shield = world.get_component<ShieldPickup>(e)) {
            gs->shield_count = std::min(gs->shield_count + shield->layers, gs->shield_max);
        }

        picked.push_back(e);
    }

    // 统一销毁已拾取道具
    for (Entity e : picked) {
        world.destroy_entity(e);
    }
}
