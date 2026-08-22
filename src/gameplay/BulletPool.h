#pragma once
#include "../core/World.h"
#include <vector>

// 子弹对象池：预分配一批实体 ID
class BulletPool {
public:
    static constexpr int POOL_SIZE = 40;  // 池子容量（同屏最多子弹数）

    BulletPool() = default;

    void preallocate(World& world);        // 初始化：创建 POOL_SIZE 个空实体 + 全部置为未占用
    Entity acquire();                      // 拿一个空闲实体（满则返回 INVALID_ENTITY）
    bool is_pooled(Entity e) const;        // 判断某实体是否属于该池

    friend void release_bullet_entity(World& world, Entity e);

private:
    // release 私有：禁止外部直接"只标记空闲而不拆组件"，
    // 否则下次 acquire 拿到的实体带旧 Transform/ElementPayload 等脏数据
    void release(Entity e);                // 仅归还实体（标记空闲，等下次复用）

    std::vector<Entity> m_entities;        // 全部池化实体 ID（长度固定 POOL_SIZE）
    std::vector<bool>   m_in_use;          // 对应下标是否已被占用（true=在用）
};

extern BulletPool g_bullet_pool;  // 全局子弹池实例（InputSystem acquire，其他系统通过 release_bullet_entity 归还）

// 共享工具：把一个子弹实体归还到对象池
//   - 池子成员：移除全部子弹组件 + pool.release（友元内部调用）
//   - 非池子成员(fallback)：直接 world.destroy_entity
void release_bullet_entity(World& world, Entity e);
