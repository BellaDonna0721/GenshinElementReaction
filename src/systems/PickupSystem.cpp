#include "PickupSystem.h"
#include "../core/World.h"
#include "../components/ShieldPickup.h"
#include "../components/singletonComponents/HitEventQueue.h"
#include "../components/singletonComponents/GameState.h"
#include <vector>
#include <algorithm>

void PickupSystem::update(World& world, float dt) {
    (void)dt;

    HitEventQueue* hit_queue = world.get_component<HitEventQueue>(0);
    GameState* gs = world.get_component<GameState>(0);
    if (!hit_queue || !gs) return;

    // 收集本帧被拾取的道具（遍历中不销毁，避免迭代器失效）
    std::vector<Entity> picked;

    // 碰撞事实由 CollisionSystem 唯一写入 PlayerTouchPickup 事件，
    // 本系统只消费：执行道具效果并标记销毁，不再自研圆-圆检测。
    for (const auto& ev : hit_queue->events) {
        if (ev.type != HitEventQueue::Type::PlayerTouchPickup) continue;

        // 护盾晶片
        if (auto* shield = world.get_component<ShieldPickup>(ev.victim)) {
            gs->shield_count = std::min(gs->shield_count + shield->layers, gs->shield_max);
        }

        picked.push_back(ev.victim);
    }

    // 统一销毁已拾取道具
    for (Entity e : picked) {
        world.destroy_entity(e);
    }
}
