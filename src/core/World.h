#pragma once
#include "Entity.h"
#include "ComponentRegistry.h"
#include "ComponentPool.h"
#include "../systems/System.h"
#include "../utils/ElementType.h"
#include <memory>
#include <vector>

// =====================================================================
//  World —— ECS 协调器（Coordinator）
//
//  职责：
//    · 实体生命周期（create/destroy）
//    · 组件增删查（委托给各 ComponentPool<T>）
//    · 组件掩码查询（query<T...> 按位筛选实体）
//    · 系统注册与每帧调度
//    · 全局玩家射击元素状态
//
// =====================================================================

class World {
public:
    World();
    ~World();

    Entity create_entity();
    void   destroy_entity(Entity e);
    bool   is_alive(Entity e) const;

    template<typename T>
    void add_component(Entity e, const T& comp);

    template<typename T>
    void remove_component(Entity e);

    template<typename T>
    T* get_component(Entity e);

    template<typename T>
    bool has_component(Entity e) const;

    template<typename... Args>
    std::vector<Entity> query();

    void add_system(std::unique_ptr<System> system);
    void update(float dt);

    // ===== 全局玩家状态：当前选中的射击元素（唯一实例，不归属于任何实体组件）=====
    ElementType get_player_current_element() const { return m_player_current_element; }
    void set_player_current_element(ElementType t) { m_player_current_element = t; }

private:
    template<typename T>
    ComponentPool<T>& get_pool();

    std::vector<std::unique_ptr<IComponentPool>> m_pools;   // 下标=ComponentTypeID
    std::vector<ComponentMask> m_masks; // 下标=EntityID，值=该实体的组件位掩码
    std::vector<Entity> m_free_ids; // 用于存储已销毁实体的 ID，按创建顺序
    Entity m_next_id = 1; // 下一个可用的实体 ID

    std::vector<std::unique_ptr<System>> m_systems; // 系统列表

    // 玩家当前选中的射击元素（初始默认 Pyro）
    ElementType m_player_current_element = ElementType::Pyro;
};

template<typename T>
void World::add_component(Entity e, const T& comp) {
    get_pool<T>().add(e, comp);
    m_masks[e] |= get_component_mask<T>();
}

template<typename T>
void World::remove_component(Entity e) {
    get_pool<T>().remove(e);
    m_masks[e] &= ~get_component_mask<T>();
}

template<typename T>
T* World::get_component(Entity e) {
    return get_pool<T>().get(e);
}

template<typename T>
bool World::has_component(Entity e) const {
    if (e >= m_masks.size()) return false;
    return (m_masks[e] & get_component_mask<T>()) != 0;
}

template<typename T>
ComponentPool<T>& World::get_pool() {
    ComponentTypeID id = get_component_type_id<T>();
    if (id >= m_pools.size()) {
        m_pools.resize(id + 1);
    }
    if (!m_pools[id]) {
        m_pools[id] = std::make_unique<ComponentPool<T>>();
    }
    return *static_cast<ComponentPool<T>*>(m_pools[id].get());
}

template<typename... Args>
std::vector<Entity> World::query() {
    std::vector<Entity> result;
    ComponentMask required = make_mask<Args...>();
    for (Entity e = 1; e < static_cast<Entity>(m_masks.size()); ++e) {
        if (m_masks[e] != 0 && (m_masks[e] & required) == required) {
            result.push_back(e);
        }
    }
    return result;
}
