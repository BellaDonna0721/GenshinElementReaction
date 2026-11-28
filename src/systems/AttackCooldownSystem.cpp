#include "AttackCooldownSystem.h"
#include "../core/World.h"
#include "../components/AttackCooldown.h"

void AttackCooldownSystem::update(World& world, float dt) {
    for (auto e : world.query<AttackCooldown>()) {
        auto* cd = world.get_component<AttackCooldown>(e);
        if (cd && cd->remaining > 0.0f) {
            cd->remaining -= dt;
        }
    }
}
