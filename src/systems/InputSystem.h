#pragma once
#include "../core/World.h"
#include "../components/ElementStatus.h"
#include <vector>

// 输入状态快照（TS 侧每帧写入，C++ 侧只读）
struct InputState {
    bool move_up         = false;  //W 键按下
    bool move_down       = false;  //S 键按下
    bool move_left       = false;  //A 键按下
    bool move_right      = false;  //D 键按下
    float mouse_x        = 0.0f;   //鼠标世界坐标 X（像素）
    float mouse_y        = 0.0f;   //鼠标世界坐标 Y（像素）
    bool mouse_click     = false;  //鼠标左键按下（Weak 子弹）
    bool mouse_right_click = false;//鼠标右键按下（Strong 子弹）
    int  element_key     = 0;      //数字键 1-7 切角色元素
};

extern InputState g_input;  //全局输入实例（wasm_api.cpp 写，InputSystem 读）

// 子弹对象池：预分配一批实体 ID，运行时 acquire/release，避免 malloc/free
class BulletPool {
public:
    static constexpr int POOL_SIZE = 40;  //池子容量（同屏最多子弹数）

    BulletPool() = default;

    void preallocate(World& world);        //初始化：创建 40 个空实体 + 全部销毁态
    Entity acquire();                      //拿一个空闲实体（满则返回 INVALID_ENTITY）
    void release(Entity e);                //归还实体（标记空闲，等下次复用）
    bool is_pooled(Entity e) const;        //判断某实体是否属于该池

private:
    std::vector<Entity> m_entities;        //全部池化实体 ID（长度固定 POOL_SIZE）
    std::vector<bool>   m_in_use;          //对应下标是否已被占用（true=在用）
};

extern BulletPool g_bullet_pool;  //全局子弹池（InputSystem 拿，其他系统归还）

// 共享工具：把一个子弹实体归还到对象池
//   - 池子成员：移除全部子弹组件 + pool.release
//   - 非池子成员(fallback)：直接 world.destroy_entity
// 调用者：LifetimeSystem(寿命到) / CollisionSystem(命中) / MovementSystem(飞出边界)
void release_bullet_entity(World& world, Entity e);

// 输入系统：第 1 步流水线（每帧第一个跑）
//   读 g_input → 写玩家 Velocity/FacingDirection → 点击冷却 → fire() 生成子弹
class InputSystem : public System {
public:
    void update(World& world, float dt) override;

private:
    float m_fire_cooldown = 0.0f;                //发射冷却剩余时间（秒），<=0 才能射
    static constexpr float FIRE_INTERVAL = 0.15f;//发射间隔（秒），≈每秒 6.6 发

    // 数字键 1-7 → 对应 ElementType 枚举值（数组下标即按键，索引 0=不用）
    // 1=Pyro火 2=Hydro水 3=Electro雷 4=Cryo冰 5=Anemo风 6=Geo岩 7=Dendro草
    static constexpr ElementType s_key_to_elem[8] = {
        ElementType::None,    // 0 占位
        ElementType::Pyro,    // 1 火
        ElementType::Hydro,   // 2 水
        ElementType::Electro, // 3 雷
        ElementType::Cryo,    // 4 冰
        ElementType::Anemo,   // 5 风
        ElementType::Geo,     // 6 岩
        ElementType::Dendro,  // 7 草
    };

    // 发射一颗子弹：从 BulletPool 拿实体 → 装配 7 个组件
    //   elem     = 子弹携带的元素（决定 ElementPayload + 子弹颜色）
    //   strength = 元素强度档（Weak / Medium / Strong），默认 Weak 保持左键兼容
    void fire(World& world, float from_x, float from_y, float dir_x, float dir_y,
              ElementType elem, ElementStrength strength = ElementStrength::Weak);
};
