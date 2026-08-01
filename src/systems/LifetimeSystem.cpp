#include "LifetimeSystem.h"
#include "../components/Lifetime.h"
#include "../components/IdentityTag.h"
#include "InputSystem.h"

void LifetimeSystem::update(World& world, float dt) {
    std::vector<Entity> expired;

    for (auto e : world.query<Lifetime>()) {
        auto* l = world.get_component<Lifetime>(e);
        l->remaining -= dt;
        if (l->remaining <= 0.0f) {
            expired.push_back(e);
        }
    }

    for (auto e : expired) {
        bool is_bullet = false;
        auto* id_tag = world.get_component<IdentityTag>(e);
        if (id_tag && id_tag->identity_type == Identity::Bullet) is_bullet = true;

        if (is_bullet) {
            // 子弹：归还到对象池
            release_bullet_entity(world, e);
        } else {
            // 其他临时实体：直接销毁（AoE、特效等未来扩展用）
            world.destroy_entity(e);
        }
    }
}
