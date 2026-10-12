#include "EnemyManager.h"
#include <algorithm>

EnemyManager g_enemy_manager;

// ========== 生成敌人 ==========
Entity EnemyManager::spawn(World& world, const EnemyPreset& preset, float x, float y) {
    Entity e = spawn_enemy(world, x, y, preset);
    if (e != INVALID_ENTITY) {
        m_enemies.push_back(e);
    }
    return e;
}

// ========== 移除指定敌人（关卡脚本手动调用）==========
void EnemyManager::remove(World& world, Entity e) {
    world.destroy_entity(e);
    unregister(e);
}

// ========== 清空所有敌人 ==========
void EnemyManager::clear_all(World& world) {
    for (Entity e : m_enemies) {
        world.destroy_entity(e);
    }
    m_enemies.clear();
}

// ========== 敌人死亡回调（DamageSystem 调用）==========
void EnemyManager::on_enemy_died(World& world, Entity e) {
    world.destroy_entity(e);
    unregister(e);
}

// ========== 内部：从存活列表注销 ==========
void EnemyManager::unregister(Entity e) {
    auto it = std::find(m_enemies.begin(), m_enemies.end(), e);
    if (it != m_enemies.end()) {
        m_enemies.erase(it);
    }
}
