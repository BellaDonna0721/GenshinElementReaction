#include "LevelSystem.h"
#include "../core/World.h"
#include "../components/Transform.h"
#include "../components/IdentityTag.h"
#include "../components/singletonComponents/GameState.h"
#include "../components/Health.h"
#include "../components/Velocity.h"
#include "../components/RenderInfo.h"
#include "../utils/element_color.h"
#include "../gameplay/SpawnPreset.h"
#include "../gameplay/EnemyManager.h"
#include "../gameplay/LevelLoader.h"
#include "../gameplay/BuffRegistry.h"
#include "PathfinderSystem.h"
#include "../utils/Random.h"
#include <algorithm>

// 根据 preset_id 获取敌人预设（与 wasm_spawn_enemy 的映射一致）
static const EnemyPreset* get_preset_by_id(int preset_id) {
    switch (preset_id) {
        case 0: return &ENEMY_PYRO;
        case 1: return &ENEMY_HYDRO;
        case 2: return &ENEMY_CRYO;
        case 3: return &ENEMY_BOSS;
        default: return nullptr;
    }
}

// 由 gs->level 推导当前关卡在 levels 数组中的索引（0-based）
// 单一权威来源：所有关卡索引只从这里取，禁止各处自行 (level-1)%size 散写
int LevelSystem::level_index(World& world) const {
    const auto& levels = get_levels();
    if (levels.empty()) return 0;
    const GameState* gs = world.get_component<GameState>(0);
    int lv = gs ? gs->level : 1;
    int sz = static_cast<int>(levels.size());
    int idx = (lv - 1) % sz;
    if (idx < 0) idx += sz;  // 防御：level 异常为负时回绕
    return idx;
}

void LevelSystem::reset(World& world) {
    GameState* gs = world.get_component<GameState>(0);
    if (gs) {
        gs->phase          = GamePhase::Playing;
        gs->level          = 1;
        gs->score          = 0;
        gs->level_cleared  = false;
        gs->active_buffs.clear();
        gs->buff_choices.clear();
        gs->shield_count   = 0;
        gs->shield_max     = 5;
        gs->is_training    = false;  // 进入关卡模式
        // 波次进度重置
        gs->wave_index     = 0;
        gs->wave_timer     = 0.0f;
        gs->wave_spawned   = false;
        gs->all_waves_done = false;
    }

    load_level(world, 1, true);
}

void LevelSystem::load_level(World& world, int level_number, bool reset_hp) {
    const auto& levels = get_levels();
    if (levels.empty()) return;

    GameState* gs = world.get_component<GameState>(0);

    // 先写入对外显示的关卡编号，后续关卡索引从 gs->level 推导（单一权威来源）
    if (gs) {
        gs->level         = level_number;
        gs->phase         = GamePhase::Playing;
        gs->level_cleared = false;
        gs->buff_choices.clear();
    }

    const LevelConfig& cfg = levels[level_index(world)];

    // 重置波次状态（写回 GameState）
    if (gs) {
        gs->wave_index     = 0;
        gs->wave_timer     = cfg.waves.empty() ? 0.0f : cfg.waves[0].delay;
        gs->wave_spawned   = false;
        gs->all_waves_done = cfg.waves.empty();
    }

    // 清空旧敌人
    g_enemy_manager.clear_all(world);

    // 生成墙体：先清空旧墙体，再按配置生成
    // 墙体是静态实体，逐个销毁
    std::vector<Entity> walls;
    for (auto e : world.query<IdentityTag>()) {
        auto* id = world.get_component<IdentityTag>(e);
        if (id && id->identity_type == Identity::Wall) walls.push_back(e);
    }
    for (auto w : walls) world.destroy_entity(w);

    // 清空场上所有道具（结晶晶片等），避免跨关卡残留
    std::vector<Entity> pickups;
    for (auto e : world.query<IdentityTag>()) {
        auto* id = world.get_component<IdentityTag>(e);
        if (id && id->identity_type == Identity::Pickup) pickups.push_back(e);
    }
    for (auto p : pickups) world.destroy_entity(p);

    for (const auto& wd : cfg.walls) {
        spawn_wall(world, wd.x, wd.y, wd.w, wd.h);
    }

    // 定位玩家并重置状态
    for (auto e : world.query<Transform, IdentityTag>()) {
        auto* id = world.get_component<IdentityTag>(e);
        if (id && id->identity_type == Identity::Player) {
            auto* t = world.get_component<Transform>(e);
            if (t) {
                t->x = cfg.player_spawn.x;
                t->y = cfg.player_spawn.y;
            }
            // 重置玩家元素为火（和 spawn_player 默认一致）
            if (gs) gs->player_current_element = ElementType::Pyro;
            auto* ri = world.get_component<RenderInfo>(e);
            if (ri) {
                ElementColor c = get_element_color(ElementType::Pyro);
                ri->color_r = c.r;
                ri->color_g = c.g;
                ri->color_b = c.b;
            }
            // 重置玩家血量（仅重新开始时重置为满血；过关时继承当前血量）
            Health* hp = world.get_component<Health>(e);
            if (hp) {
                if (reset_hp) hp->current_hp = hp->max_hp;
                hp->invincible_timer = 0.0f;
            }
            break;
        }
    }

    // === 构建路径网格 ===
    // 关卡墙矩形 → PathfinderSystem 内部 grid，Boss Chase 态 A* 依赖
    PathfinderSystem::build_grid(cfg.walls);
}

void LevelSystem::start_wave(World& world) {
    GameState* gs = world.get_component<GameState>(0);
    if (!gs) return;

    const auto& levels = get_levels();
    const LevelConfig& cfg = levels[level_index(world)];
    if (gs->wave_index >= static_cast<int>(cfg.waves.size())) {
        gs->all_waves_done = true;
        return;
    }

    // 一次性刷出本波所有敌人（每个 spawn 位置固定一只）
    const WaveConfig& wave = cfg.waves[gs->wave_index];
    for (const auto& sp : wave.spawns) {
        const EnemyPreset* preset = get_preset_by_id(sp.preset_id);
        if (preset) {
            g_enemy_manager.spawn(world, *preset, sp.x, sp.y);
        }
    }

    // 标记本波已刷完，等待玩家杀光后再推进下一波
    gs->wave_index++;
    gs->wave_spawned = true;
}

void LevelSystem::update(World& world, float dt) {
    GameState* gs = world.get_component<GameState>(0);
    if (!gs) return;

    // 训练模式下不运行关卡逻辑
    if (gs->is_training) return;

    // 非 Playing 阶段不处理
    if (gs->phase != GamePhase::Playing) return;
    if (gs->level_cleared) return;

    const auto& levels = get_levels();
    if (levels.empty()) return;
    const LevelConfig& cfg = levels[level_index(world)];

    // ========== 波次刷怪 ==========
    // 状态机：
    //   A) 本波已刷完，等待敌人全部消灭 → alive_count()==0 后进入 B
    //   B) 扣 delay 倒计时 → 归零后 start_wave 刷本波，回到 A
    if (!gs->all_waves_done) {
        if (gs->wave_spawned) {
            // 等本波敌人全灭，再开始下一波 delay
            if (g_enemy_manager.alive_count() == 0) {
                if (gs->wave_index < static_cast<int>(cfg.waves.size())) {
                    gs->wave_timer = cfg.waves[gs->wave_index].delay;
                    gs->wave_spawned = false;
                } else {
                    // 最后一波也刷完且敌人全灭
                    gs->all_waves_done = true;
                }
            }
        } else {
            // 扣 delay 倒计时，归零刷本波
            gs->wave_timer -= dt;
            if (gs->wave_timer <= 0.0f) {
                start_wave(world);
            }
        }
    }

    // ========== 通关判定 ==========
    if (gs->all_waves_done && g_enemy_manager.alive_count() == 0) {
        complete_level(world);
    }
}

void LevelSystem::complete_level(World& world) {
    GameState* gs = world.get_component<GameState>(0);
    if (!gs) return;
    if (gs->level_cleared) return;  // 幂等：同一帧只触发一次通关
    gs->level_cleared = true;

    const auto& levels = get_levels();
    const LevelConfig& cfg = levels[level_index(world)];

    if (cfg.is_boss) {
        // Boss 关：进入 LevelComplete，等待选增益
        gs->phase = GamePhase::LevelComplete;
        pick_buff_choices(world);
    } else {
        // 普通关：直接进入下一关
        load_level(world, gs->level + 1);
    }
}

void LevelSystem::pick_buff_choices(World& world) {
    GameState* gs = world.get_component<GameState>(0);
    if (!gs) return;

    // 从所有增益中随机选 3 个不重复的
    std::vector<int> indices;
    for (int i = 0; i < BuffRegistry::count(); i++) indices.push_back(i);

    // Fisher-Yates 洗牌取前 3
    for (int i = static_cast<int>(indices.size()) - 1; i > 0; i--) {
        int j = game_random::range_int(0, i);
        std::swap(indices[i], indices[j]);
    }

    gs->buff_choices.clear();
    for (int i = 0; i < 3 && i < static_cast<int>(indices.size()); i++) {
        gs->buff_choices.push_back(static_cast<BuffType>(indices[i]));
    }
}

void LevelSystem::apply_buff_and_next(World& world, BuffType buff) {
    GameState* gs = world.get_component<GameState>(0);
    if (!gs) return;

    gs->active_buffs.push_back(buff);

    // 找到玩家
    Entity player = INVALID_ENTITY;
    for (auto e : world.query<Health, IdentityTag>()) {
        auto* id = world.get_component<IdentityTag>(e);
        if (id && id->identity_type == Identity::Player) { player = e; break; }
    }

    // 即时生效类增益
    if (player != INVALID_ENTITY) {
        Health* hp = world.get_component<Health>(player);
        if (hp) {
            switch (buff) {
                case BuffType::MaxHpUp:
                    hp->max_hp += BuffRegistry::get(buff).value;
                    hp->current_hp = hp->max_hp;
                    break;
                case BuffType::Heal:
                    hp->current_hp = std::min(hp->max_hp, hp->current_hp + hp->max_hp * BuffRegistry::get(buff).value);
                    break;
                case BuffType::Shield:
                    gs->shield_max++;
                    break;
                default:
                    break;
            }
        }
        // 移速增益：修改玩家 Velocity.speed（不依赖 Health，独立处理）
        if (buff == BuffType::MoveSpeedUp) {
            Velocity* vel = world.get_component<Velocity>(player);
            if (vel) vel->speed *= (1.0f + BuffRegistry::get(buff).value);
        }
    }

    // 进入下一关
    load_level(world, gs->level + 1);
}
