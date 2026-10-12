#include <emscripten.h>
#include "../core/World.h"
#include "../systems/InputSystem.h"
#include "../systems/LifetimeSystem.h"
#include "../systems/MovementSystem.h"
#include "../systems/CollisionSystem.h"
#include "../systems/ElementReactionSystem.h"
#include "../systems/DamageSystem.h"
#include "../systems/OutputSystem.h"
#include "../gameplay/SpawnPreset.h"
#include "../gameplay/BulletPool.h"
#include "../gameplay/EnemyManager.h"
#include "../components/DamageQueue.h"
#include "../components/DamageNumberQueue.h"

static World* g_world = nullptr;

extern "C" {

EMSCRIPTEN_KEEPALIVE
void wasm_init() {
    if (g_world) return;
    g_world = new World();

    // 系统注册顺序即每帧执行流水线：
    //   1. 读输入 → 写入速度/朝向/生成新子弹(add 组件)
    //   2. 生命周期扣减 → 寿命到的子弹回收
    //   3. 物理移动 → 所有带 Velocity 的实体前进 + 越界回收/夹紧
    //   4. 碰撞检测 → 子弹命中敌人 → 写 pending_element/gauge + 直射伤害事件 + 命中子弹回收
    //   5. 元素反应 → 处理 pending → 反应触发 + 衰减 + 增幅反应伤害事件
    //   6. 伤害结算 → 消费伤害队列扣血 + 死亡处理 + 飘字
    //   7. 渲染输出 → 收 RenderCommand + 血条 + 飘字给 TS 侧
    g_world->add_system(std::make_unique<InputSystem>());
    g_world->add_system(std::make_unique<LifetimeSystem>());
    g_world->add_system(std::make_unique<MovementSystem>());
    g_world->add_system(std::make_unique<CollisionSystem>());
    g_world->add_system(std::make_unique<ElementReactionSystem>());
    g_world->add_system(std::make_unique<DamageSystem>());
    g_world->add_system(std::make_unique<OutputSystem>());

    // 对象池：预创建 40 个子弹实体，运行时无 malloc，不破坏 ECS
    g_bullet_pool.preallocate(*g_world);

    // —— 全局单例组件挂在 Entity 0（保留实体，永不被 create_entity 分配）——
    //   必须在 preallocate 之后：create_entity 会把 m_masks 扩容到至少 41，
    //   这样 m_masks[0] 才合法可写。
    {
        DamageQueue dq;
        g_world->add_component<DamageQueue>(0, dq);

        DamageNumberQueue dnq;
        g_world->add_component<DamageNumberQueue>(0, dnq);
    }

    spawn_player(*g_world, 200.0f, 350.0f);
    g_enemy_manager.spawn(*g_world, ENEMY_HYDRO, 580.0f, 180.0f);
    g_enemy_manager.spawn(*g_world, ENEMY_HYDRO, 680.0f, 360.0f);
    g_enemy_manager.spawn(*g_world, ENEMY_HYDRO, 560.0f, 530.0f);
}

EMSCRIPTEN_KEEPALIVE
void wasm_update(float dt) {
    if (!g_world) return;
    g_world->update(dt);
}

EMSCRIPTEN_KEEPALIVE
void wasm_set_input(int move_up, int move_down,
                    int move_left, int move_right,
                    float mouse_x, float mouse_y,
                    int mouse_click,
                    int mouse_right_click,
                    int element_key)
{
    g_input.move_up          = move_up != 0;
    g_input.move_down        = move_down != 0;
    g_input.move_left        = move_left != 0;
    g_input.move_right       = move_right != 0;
    g_input.mouse_x          = mouse_x;
    g_input.mouse_y          = mouse_y;
    g_input.mouse_click      = mouse_click != 0;
    g_input.mouse_right_click = mouse_right_click != 0;
    g_input.element_key      = element_key;  // 1-7 切换玩家元素（1火…7草），0=不切
}

// ========== 渲染数据导出 ==========

EMSCRIPTEN_KEEPALIVE
RenderCommand* wasm_get_render_data() {
    return g_render_commands.data();
}

EMSCRIPTEN_KEEPALIVE
int wasm_get_render_count() {
    return static_cast<int>(g_render_commands.size());
}

EMSCRIPTEN_KEEPALIVE
HealthBarInfo* wasm_get_health_data() {
    return g_health_bars.data();
}

EMSCRIPTEN_KEEPALIVE
int wasm_get_health_count() {
    return static_cast<int>(g_health_bars.size());
}

EMSCRIPTEN_KEEPALIVE
DamageNumberInfo* wasm_get_damage_numbers() {
    return g_damage_numbers.data();
}

EMSCRIPTEN_KEEPALIVE
int wasm_get_damage_number_count() {
    return static_cast<int>(g_damage_numbers.size());
}

// ========== 敌人管理（自定义关卡用）==========

EMSCRIPTEN_KEEPALIVE
int wasm_spawn_enemy(int preset_id, float x, float y) {
    if (!g_world) return static_cast<int>(INVALID_ENTITY);
    const EnemyPreset* preset = nullptr;
    switch (preset_id) {
        case 0: preset = &ENEMY_PYRO;  break;
        case 1: preset = &ENEMY_HYDRO; break;
        case 2: preset = &ENEMY_CRYO;  break;
        default: return static_cast<int>(INVALID_ENTITY);
    }
    Entity e = g_enemy_manager.spawn(*g_world, *preset, x, y);
    return static_cast<int>(e);
}

EMSCRIPTEN_KEEPALIVE
void wasm_remove_enemy(int entity_id) {
    if (!g_world) return;
    g_enemy_manager.remove(*g_world, static_cast<Entity>(entity_id));
}

EMSCRIPTEN_KEEPALIVE
void wasm_clear_enemies() {
    if (!g_world) return;
    g_enemy_manager.clear_all(*g_world);
}

EMSCRIPTEN_KEEPALIVE
int wasm_get_alive_enemy_count() {
    return g_enemy_manager.alive_count();
}

EMSCRIPTEN_KEEPALIVE
void wasm_reset() {
    delete g_world;
    g_world = nullptr;
    wasm_init();
}

}
