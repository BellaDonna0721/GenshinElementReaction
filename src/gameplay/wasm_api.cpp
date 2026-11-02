#include <emscripten.h>
#include "../core/World.h"
#include "../systems/InputSystem.h"
#include "../systems/LifetimeSystem.h"
#include "../systems/MovementSystem.h"
#include "../systems/EnemyAISystem.h"
#include "../systems/CollisionSystem.h"
#include "../systems/ElementReactionSystem.h"
#include "../systems/DamageSystem.h"
#include "../systems/LevelSystem.h"
#include "../systems/OutputSystem.h"
#include "../gameplay/SpawnPreset.h"
#include "../gameplay/BulletPool.h"
#include "../gameplay/EnemyManager.h"
#include "../components/DamageQueue.h"
#include "../components/DamageNumberQueue.h"
#include "../components/GameState.h"
#include "../components/Health.h"
#include "../components/IdentityTag.h"
#include <vector>

static World* g_world = nullptr;
static LevelSystem* g_level_system = nullptr;

extern "C" {

EMSCRIPTEN_KEEPALIVE
void wasm_init() {
    if (g_world) return;
    g_world = new World();

    // 系统执行流水线：
    //   1. Input      — 读输入、写速度/朝向、生成子弹
    //   2. Lifetime   — 子弹寿命扣减、回收
    //   3. Movement   — 所有带 Velocity 的实体移动
    //   4. EnemyAI    — 敌人状态机：算速度（巡逻/警戒/战斗/逃跑）
    //   5. Collision  — 子弹-敌人命中、敌人-玩家接触伤害、墙体推开
    //   6. ElementReaction — 元素反应、衰减、增幅伤害
    //   7. Damage     — 消费伤害队列、扣血、死亡、加分
    //   8. Level      — 波次刷怪、通关判定、增益
    //   9. Output     — 打包渲染数据
    g_world->add_system(std::make_unique<InputSystem>());
    g_world->add_system(std::make_unique<LifetimeSystem>());
    g_world->add_system(std::make_unique<MovementSystem>());
    g_world->add_system(std::make_unique<EnemyAISystem>());
    g_world->add_system(std::make_unique<CollisionSystem>());
    g_world->add_system(std::make_unique<ElementReactionSystem>());
    g_world->add_system(std::make_unique<DamageSystem>());
    auto* level_sys = new LevelSystem();
    g_level_system = level_sys;
    g_world->add_system(std::unique_ptr<System>(level_sys));
    g_world->add_system(std::make_unique<OutputSystem>());

    // 对象池：预创建 40 个子弹实体
    g_bullet_pool.preallocate(*g_world);

    // 全局单例组件挂在 Entity 0
    {
        DamageQueue dq;
        g_world->add_component<DamageQueue>(0, dq);

        DamageNumberQueue dnq;
        g_world->add_component<DamageNumberQueue>(0, dnq);

        GameState gs;
        gs.is_training = true;  // wasm_init 默认是训练模式
        g_world->add_component<GameState>(0, gs);
    }

    spawn_player(*g_world, 200.0f, 350.0f);

    // 训练模式默认场景：2 面测试墙 + 1 只无限血量史莱姆（用于练习元素反应）
    spawn_wall(*g_world, 420.0f, 200.0f, 30.0f, 220.0f);
    spawn_wall(*g_world, 530.0f, 420.0f, 200.0f, 30.0f);
    Entity dummy = g_enemy_manager.spawn(*g_world, ENEMY_HYDRO, 650.0f, 350.0f);
    // 训练木桩：血量拉到极大值，模拟无限血
    Health* dhp = g_world->get_component<Health>(dummy);
    if (dhp) {
        dhp->max_hp     = 999999.0f;
        dhp->current_hp = 999999.0f;
    }
}

// 开始游戏：从第 1 关开始（关卡模式）
EMSCRIPTEN_KEEPALIVE
void wasm_start_game() {
    if (!g_world) wasm_init();
    if (g_level_system) g_level_system->reset(*g_world);
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
    g_input.element_key      = element_key;
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
EMSCRIPTEN_KEEPALIVE
WallRenderInfo* wasm_get_wall_data() {
    return g_walls.data();
}
EMSCRIPTEN_KEEPALIVE
int wasm_get_wall_count() {
    return static_cast<int>(g_walls.size());
}

// ========== 敌人管理 ==========

EMSCRIPTEN_KEEPALIVE
int wasm_spawn_enemy(int preset_id, float x, float y) {
    if (!g_world) return static_cast<int>(INVALID_ENTITY);
    const EnemyPreset* preset = nullptr;
    switch (preset_id) {
        case 0: preset = &ENEMY_PYRO;  break;
        case 1: preset = &ENEMY_HYDRO; break;
        case 2: preset = &ENEMY_CRYO;  break;
        case 3: preset = &ENEMY_BOSS;  break;
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

// ========== 游戏状态 / 关卡 / 增益 ==========

// 返回游戏阶段：0=Playing 1=LevelComplete 2=GameOver
EMSCRIPTEN_KEEPALIVE
int wasm_get_game_phase() {
    if (!g_world) return 0;
    GameState* gs = g_world->get_component<GameState>(0);
    return gs ? static_cast<int>(gs->phase) : 0;
}

EMSCRIPTEN_KEEPALIVE
int wasm_get_score() {
    if (!g_world) return 0;
    GameState* gs = g_world->get_component<GameState>(0);
    return gs ? gs->score : 0;
}

EMSCRIPTEN_KEEPALIVE
int wasm_get_level() {
    if (!g_world) return 1;
    GameState* gs = g_world->get_component<GameState>(0);
    return gs ? gs->level : 1;
}

// 玩家当前血量比例（0-100）
EMSCRIPTEN_KEEPALIVE
int wasm_get_player_hp_ratio() {
    if (!g_world) return 100;
    for (auto e : g_world->query<Health, IdentityTag>()) {
        auto* id = g_world->get_component<IdentityTag>(e);
        if (id && id->identity_type == Identity::Player) {
            Health* hp = g_world->get_component<Health>(e);
            if (hp && hp->max_hp > 0) {
                return static_cast<int>(hp->current_hp / hp->max_hp * 100.0f);
            }
        }
    }
    return 100;
}

// 返回候选增益 ID 数组（最多 3 个），写入 out 数组，返回实际数量
EMSCRIPTEN_KEEPALIVE
int wasm_get_buff_choices(int* out) {
    if (!g_world || !out) return 0;
    GameState* gs = g_world->get_component<GameState>(0);
    if (!gs) return 0;
    int n = static_cast<int>(gs->buff_choices.size());
    for (int i = 0; i < n; i++) out[i] = static_cast<int>(gs->buff_choices[i]);
    return n;
}

// 选择增益并进入下一关
EMSCRIPTEN_KEEPALIVE
void wasm_select_buff(int buff_id) {
    if (!g_world || !g_level_system) return;
    g_level_system->apply_buff_and_next(*g_world, static_cast<BuffType>(buff_id));
}

// 已选增益数量
EMSCRIPTEN_KEEPALIVE
int wasm_get_active_buff_count() {
    if (!g_world) return 0;
    GameState* gs = g_world->get_component<GameState>(0);
    return gs ? static_cast<int>(gs->active_buffs.size()) : 0;
}

// 第 idx 个已选增益 ID
EMSCRIPTEN_KEEPALIVE
int wasm_get_active_buff(int idx) {
    if (!g_world) return -1;
    GameState* gs = g_world->get_component<GameState>(0);
    if (!gs || idx < 0 || idx >= static_cast<int>(gs->active_buffs.size())) return -1;
    return static_cast<int>(gs->active_buffs[idx]);
}

EMSCRIPTEN_KEEPALIVE
void wasm_reset() {
    delete g_world;
    g_world = nullptr;
    g_level_system = nullptr;
    wasm_init();
}

}
