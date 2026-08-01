#include <emscripten.h>
#include "../core/World.h"
#include "../systems/InputSystem.h"
#include "../systems/LifetimeSystem.h"
#include "../systems/MovementSystem.h"
#include "../systems/CollisionSystem.h"
#include "../systems/ElementReactionSystem.h"
#include "../systems/OutputSystem.h"
#include "../gameplay/SpawnPreset.h"

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
    //   4. 碰撞检测 → 子弹命中敌人 → 写 pending_element/gauge + 命中子弹回收
    //   5. 元素反应 → 处理 pending → 反应触发 + 衰减
    //   6. 渲染输出 → 收 RenderCommand 给 TS 侧
    g_world->add_system(std::make_unique<InputSystem>());
    g_world->add_system(std::make_unique<LifetimeSystem>());
    g_world->add_system(std::make_unique<MovementSystem>());
    g_world->add_system(std::make_unique<CollisionSystem>());
    g_world->add_system(std::make_unique<ElementReactionSystem>());
    g_world->add_system(std::make_unique<OutputSystem>());

    // 对象池：预创建 40 个子弹实体，运行时无 malloc，不破坏 ECS
    g_bullet_pool.preallocate(*g_world);

    spawn_player(*g_world, 200.0f, 300.0f);
    spawn_enemy(*g_world, 500.0f, 200.0f, ENEMY_HYDRO);
    spawn_enemy(*g_world, 550.0f, 350.0f, ENEMY_HYDRO);
    spawn_enemy(*g_world, 480.0f, 450.0f, ENEMY_HYDRO);
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
                    int element_key)
{
    g_input.move_up    = move_up != 0;
    g_input.move_down  = move_down != 0;
    g_input.move_left  = move_left != 0;
    g_input.move_right = move_right != 0;
    g_input.mouse_x    = mouse_x;
    g_input.mouse_y    = mouse_y;
    g_input.mouse_click = mouse_click != 0;
    g_input.element_key = element_key;  // 1-7 切换玩家元素，0=不切
}

EMSCRIPTEN_KEEPALIVE
RenderCommand* wasm_get_render_data() {
    return g_render_commands.data();
}

EMSCRIPTEN_KEEPALIVE
int wasm_get_render_count() {
    return static_cast<int>(g_render_commands.size());
}

EMSCRIPTEN_KEEPALIVE
void wasm_reset() {
    delete g_world;
    g_world = nullptr;
    wasm_init();
}

}
