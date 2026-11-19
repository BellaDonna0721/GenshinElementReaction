#include <emscripten.h>
#include "../core/World.h"
#include "../systems/InputSystem.h"
#include "../systems/LifetimeSystem.h"
#include "../systems/MovementSystem.h"
#include "../systems/EnemyAISystem.h"
#include "../systems/CollisionSystem.h"
#include "../systems/PickupSystem.h"
#include "../systems/ElementReactionSystem.h"
#include "../systems/DamageSystem.h"
#include "../systems/LevelSystem.h"
#include "../systems/OutputSystem.h"
#include "../gameplay/SpawnPreset.h"
#include "../gameplay/BulletPool.h"
#include "../gameplay/EnemyManager.h"
#include "../gameplay/LevelLoader.h"
#include "../components/singletonComponents/DamageQueue.h"
#include "../components/singletonComponents/DamageNumberQueue.h"
#include "../components/singletonComponents/HitFxQueue.h"
#include "../components/singletonComponents/GameState.h"
#include "../components/Health.h"
#include "../components/IdentityTag.h"
#include "../components/EnemyAI.h"
#include "../systems/PathfinderSystem.h"
#include "../components/Transform.h"
#include "../utils/GameConfig.h"
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
    //   6. Pickup     — 玩家拾取道具（结晶晶片等）
    //   7. ElementReaction — 元素反应、衰减、增幅伤害
    //   8. Damage     — 消费伤害队列、扣血、死亡、加分
    //   9. Level      — 波次刷怪、通关判定、增益
    //  10. Output     — 打包渲染数据
    g_world->add_system(std::make_unique<InputSystem>());
    g_world->add_system(std::make_unique<LifetimeSystem>());
    g_world->add_system(std::make_unique<MovementSystem>());
    g_world->add_system(std::make_unique<EnemyAISystem>());
    g_world->add_system(std::make_unique<CollisionSystem>());
    g_world->add_system(std::make_unique<PickupSystem>());
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

        HitFxQueue fxq;
        g_world->add_component<HitFxQueue>(0, fxq);

        GameState gs;
        gs.is_training = true;  // wasm_init 默认是训练模式
        g_world->add_component<GameState>(0, gs);
    }

    spawn_player(*g_world, 200.0f, 350.0f);

    // 训练模式默认场景：2 面测试墙 + 1 只无限血量史莱姆（用于练习元素反应）
    spawn_wall(*g_world, 420.0f, 200.0f, 30.0f, 220.0f);
    spawn_wall(*g_world, 530.0f, 420.0f, 200.0f, 30.0f);
    Entity dummy = g_enemy_manager.spawn(*g_world, ENEMY_TEST, 650.0f, 350.0f);
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

// ========== 全局配置导出 ==========

EMSCRIPTEN_KEEPALIVE
float* wasm_get_world_size() {
    static float size[2] = { GameConfig::WORLD_W, GameConfig::WORLD_H };
    return size;
}

// 从 JSON 字符串加载关卡列表（TS 侧 fetch levels.json 后传入）
EMSCRIPTEN_KEEPALIVE
void wasm_load_levels(const char* json_ptr, int len) {
    if (!json_ptr || len <= 0) return;
    try {
        set_levels(parse_levels_json(std::string(json_ptr, len)));
    } catch (...) {
        // 解析失败：保持现有关卡数据不变
    }
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
DamageNumberQueue::DamageNumber* wasm_get_damage_numbers() {
    auto* q = g_world->get_component<DamageNumberQueue>(0);
    return q ? q->numbers.data() : nullptr;
}
EMSCRIPTEN_KEEPALIVE
int wasm_get_damage_number_count() {
    auto* q = g_world->get_component<DamageNumberQueue>(0);
    return q ? static_cast<int>(q->numbers.size()) : 0;
}
EMSCRIPTEN_KEEPALIVE
WallRenderInfo* wasm_get_wall_data() {
    return g_walls.data();
}
EMSCRIPTEN_KEEPALIVE
int wasm_get_wall_count() {
    return static_cast<int>(g_walls.size());
}
EMSCRIPTEN_KEEPALIVE
HitFxQueue::HitFx* wasm_get_hit_fx_data() {
    auto* q = g_world->get_component<HitFxQueue>(0);
    return q ? q->items.data() : nullptr;
}
EMSCRIPTEN_KEEPALIVE
int wasm_get_hit_fx_count() {
    auto* q = g_world->get_component<HitFxQueue>(0);
    return q ? static_cast<int>(q->items.size()) : 0;
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

// ========== Boss 路径调试导出 ==========

// 世界坐标路径缓存（每帧更新，TS 侧读取当前 A* 规划路径做可视化）
// 2 个 float = {x, y}，最后追加当前 waypoint 索引
static std::vector<float> g_boss_path_cache;

EMSCRIPTEN_KEEPALIVE
float* wasm_get_boss_path_data() {
    if (!g_world) return nullptr;
    g_boss_path_cache.clear();

    // 调试开关关闭时不导出路径数据（前端据此不画黄线）
    if constexpr (!GameConfig::DEBUG_PATH_VIS) return g_boss_path_cache.data();

    // 遍历找第一个 Boss（带 StateMachineAI + Chase 态有有效路径）
    for (auto e : g_world->query<Transform, StateMachineAI>()) {
        auto* sm = g_world->get_component<StateMachineAI>(e);
        auto* t  = g_world->get_component<Transform>(e);
        if (!sm || !t) continue;
        if (sm->path.empty()) continue;

        // Boss 自身坐标作为路径起点
        g_boss_path_cache.push_back(t->x);
        g_boss_path_cache.push_back(t->y);

        // cell 坐标路径 → 世界坐标
        for (const auto& [cx, cy] : sm->path) {
            float wx = cx * GameConfig::PATH_CELL_SIZE + GameConfig::PATH_CELL_SIZE * 0.5f;
            float wy = cy * GameConfig::PATH_CELL_SIZE + GameConfig::PATH_CELL_SIZE * 0.5f;
            g_boss_path_cache.push_back(wx);
            g_boss_path_cache.push_back(wy);
        }
        break;  // 只取第一个 Boss
    }
    return g_boss_path_cache.data();
}

EMSCRIPTEN_KEEPALIVE
int wasm_get_boss_path_count() {
    // 注意：必须先调 wasm_get_boss_path_data() 填充缓存
    // 这里返回的是点数量（float 数 / 2）
    return static_cast<int>(g_boss_path_cache.size()) / 2;
}

// ========== Path grid 导出（调试用，墙=-1，可通行=0）==========

static std::vector<float> g_grid_cache;

EMSCRIPTEN_KEEPALIVE
float* wasm_get_costmap_data() {
    if constexpr (!GameConfig::DEBUG_PATH_VIS) return nullptr;
    g_grid_cache.clear();
    g_grid_cache.reserve(GameConfig::PATH_GRID_W * GameConfig::PATH_GRID_H);
    for (int cy = 0; cy < GameConfig::PATH_GRID_H; cy++) {
        for (int cx = 0; cx < GameConfig::PATH_GRID_W; cx++) {
            float v = PathfinderSystem::grid[cy][cx] ? 0.0f : -1.0f;
            g_grid_cache.push_back(v);
        }
    }
    return g_grid_cache.data();
}

// ========== 重置 ==========

EMSCRIPTEN_KEEPALIVE
void wasm_reset() {
    delete g_world;
    g_world = nullptr;
    g_level_system = nullptr;
    wasm_init();
}

}
