#include "BulletPool.h"
#include "../components/Transform.h"
#include "../components/Velocity.h"
#include "../components/Lifetime.h"
#include "../components/ElementPayload.h"
#include "../components/Collider.h"
#include "../components/IdentityTag.h"
#include "../components/RenderInfo.h"

BulletPool g_bullet_pool;

// ========== BulletPool ==========

void BulletPool::preallocate(World& world) {
    m_entities.clear();
    m_in_use.clear();
    for (int i = 0; i < POOL_SIZE; ++i) {
        m_entities.push_back(world.create_entity());
        m_in_use.push_back(false);
    }
}

Entity BulletPool::acquire() {
    for (int i = 0; i < (int)m_entities.size(); ++i) {
        if (!m_in_use[i]) {
            m_in_use[i] = true;
            return m_entities[i];
        }
    }
    return INVALID_ENTITY;
}

void BulletPool::release(Entity e) {
    for (int i = 0; i < (int)m_entities.size(); ++i) {
        if (m_entities[i] == e && m_in_use[i]) {
            m_in_use[i] = false;
            return;
        }
    }
}

bool BulletPool::is_pooled(Entity e) const {
    for (auto id : m_entities) if (id == e) return true;
    return false;
}

// ========== 共享工具：归还子弹实体到对象池 ==========

void release_bullet_entity(World& world, Entity e) {
    if (!g_bullet_pool.is_pooled(e)) {
        world.destroy_entity(e);
        return;
    }
    // 移除子弹发射时 add 的 7 个组件（Transform/Velocity/Lifetime/ElementPayload/Collider/IdentityTag/RenderInfo）
    world.remove_component<Transform>(e);
    world.remove_component<Velocity>(e);
    world.remove_component<Lifetime>(e);
    world.remove_component<ElementPayload>(e);
    world.remove_component<Collider>(e);
    world.remove_component<IdentityTag>(e);
    world.remove_component<RenderInfo>(e);
    g_bullet_pool.release(e);
}
