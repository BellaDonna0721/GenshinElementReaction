#pragma once
#include "Entity.h"
#include <memory>
#include <vector>
#include <utility>

// =====================================================================
//  组件存储层（稀疏集 sparse-set 实现）
//
//  职责：
//    · IComponentPool —— 类型擦除的组件池接口，让 World 能用统一容器
//      管理任意类型的 ComponentPool<T>。
//    · ComponentPool<T> —— 对单个组件类型 T 的实体→组件映射存储。
//      采用稀疏集（sparse + dense 双数组），查询 O(1)、删除 O(1)、
//      且 dense 数组连续 → 遍历时缓存友好。
//
//  独立于 World：本头只依赖 Entity.h，可被其他需要组件存储的地方复用。
// =====================================================================

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
