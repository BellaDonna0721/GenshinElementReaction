#pragma once
#include "Entity.h"
#include "ComponentRegistry.h"
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <vector>

class IComponentPool {
public:
    virtual ~IComponentPool() = default;
    virtual void remove(Entity e) = 0;
    virtual bool has(Entity e) const = 0;
};

template<typename T>
class ComponentPool : public IComponentPool {
    static constexpr int MAX_ENTITIES = 2048;

    std::vector<int>    m_sparse;
    std::vector<Entity> m_dense;
    std::vector<T>      m_components;

public:
    ComponentPool() {
        m_sparse.assign(MAX_ENTITIES, -1);
    }

    void add(Entity e, const T& comp) {
        if (e >= MAX_ENTITIES) return;
        int idx = m_sparse[e];
        if (idx >= 0) {
            m_components[idx] = comp;
            return;
        }
        m_sparse[e] = static_cast<int>(m_dense.size());
        m_dense.push_back(e);
        m_components.push_back(comp);
    }

    void remove(Entity e) override {
        if (e >= MAX_ENTITIES) return;
        int idx = m_sparse[e];
        if (idx < 0) return;

        int last = static_cast<int>(m_dense.size()) - 1;
        if (idx != last) {
            m_dense[idx] = m_dense[last];
            m_components[idx] = std::move(m_components[last]);
            m_sparse[m_dense[idx]] = idx;
        }
        m_dense.pop_back();
        m_components.pop_back();
        m_sparse[e] = -1;
    }

    bool has(Entity e) const override {
        return e < MAX_ENTITIES && m_sparse[e] >= 0;
    }

    T* get(Entity e) {
        if (e >= MAX_ENTITIES) return nullptr;
        int idx = m_sparse[e];
        return idx >= 0 ? &m_components[idx] : nullptr;
    }
};

class System {
public:
    virtual ~System() = default;
    virtual void update(class World& world, float dt) = 0;
};

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

private:
    template<typename T>
    ComponentPool<T>& get_pool();

    std::vector<std::unique_ptr<IComponentPool>> m_pools;   // 下标=ComponentTypeID
    std::vector<ComponentMask> m_masks; // 下标=EntityID，值=该实体的组件位掩码
    std::vector<Entity> m_free_ids; // 用于存储已销毁实体的 ID，按创建顺序
    Entity m_next_id = 1; // 下一个可用的实体 ID

    std::vector<std::unique_ptr<System>> m_systems; // 系统列表
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