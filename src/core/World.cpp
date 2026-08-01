#include "World.h"

World::World()  = default;
World::~World() = default;

Entity World::create_entity() {
    Entity e;
    if (!m_free_ids.empty()) {
        e = m_free_ids.back();
        m_free_ids.pop_back();
    } else {
        e = m_next_id++;
    }
    if (e >= m_masks.size()) {
        m_masks.resize(e + 1, 0);
    }
    m_masks[e] = 0;
    return e;
}

void World::destroy_entity(Entity e) {
    if (e >= m_masks.size() ||                      // 从未分配到这么大
        m_masks[e] == 0) {                          // 已经销毁/未激活
        return;
    }
    for (auto& pool : m_pools) {
        if (pool) pool->remove(e);
    }
    m_masks[e] = 0;
    m_free_ids.push_back(e);
}

bool World::is_alive(Entity e) const {
    return e < m_masks.size() && m_masks[e] != 0;
}

void World::update(float dt) {
    for (auto& sys : m_systems) {
        sys->update(*this, dt);
    }
}

void World::add_system(std::unique_ptr<System> system) {
    m_systems.push_back(std::move(system));
}