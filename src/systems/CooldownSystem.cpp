#include "CooldownSystem.h"
#include "../core/World.h"
#include "../components/Cooldown.h"

void CooldownSystem::update(World& world, float dt) {
    for (auto e : world.query<Cooldown>()) {
        auto* cd = world.get_component<Cooldown>(e);
        if (cd && cd->remaining > 0.0f) {
            cd->remaining -= dt;
        }
    }
}
