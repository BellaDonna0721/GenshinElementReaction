#include "EnemyAISystem.h"
#include "../core/World.h"
#include "../components/Transform.h"
#include "../components/Velocity.h"
#include "../components/EnemyAI.h"
#include "../components/StateMachineAI.h"
#include "../components/PatrolAI.h"
#include "../components/PathFollowAI.h"
#include "../components/ObstacleContact.h"
#include "../components/Health.h"
#include "../components/IdentityTag.h"
#include "../components/ElementStatus.h"
#include "../utils/GameConfig.h"
#include "../utils/Random.h"
#include "PathfinderSystem.h"
#include <cmath>
#include <numbers>

namespace {

// ===== Chase 路径跟随参数 =====
constexpr float PATH_REPATH_CD   = 0.25f;  // 重算最小间隔（秒），节流避免贴墙每帧抖动
constexpr float PATH_STUCK_TIME  = 0.4f;   // 持续被挡多久后开始判定卡死
constexpr float PATH_STUCK_DIST  = 30.0f;  // 卡死窗口内净位移小于此值才算真卡死（px）
constexpr float PATH_ESCAPE_TIME = 0.6f;  // 脱困态锁定方向的持续时长（秒）
constexpr float PATH_LOOKAHEAD   = 80.0f;  // 前瞻距离：选该距离内最远的可达航点
constexpr float PATH_ARRIVE_R    = 18.0f;  // 航点到达半径
constexpr float PATH_TURN_RATE   = 8.0f;   // 转向平滑速率（/秒，指数插值）
constexpr float PATH_PUSH_BASE   = 1.2f;   // 正常贴墙时推开方向揉进转向的权重（轻量滑动）

// 巡逻撞墙避让：沿推开方向反向走多远再重新随机选点
constexpr float PATROL_WALL_AVOID_DIST = 120.0f;

// 巡逻选点：候选点被 leash 拒绝时的最大重试次数（受控重试，避免重算风暴）
constexpr int PATROL_PICK_TRIES = 8;

// 从 io_index 开始推进已到达航点，并在 lookahead 内选最远的、从当前位置可达的航点。
// 可达判定用 segment_walkable（世界坐标采样），即使 Boss 本体压在膨胀格上也能正确取点。
// 返回 false 表示路径已走完（调用方应直奔玩家）。io_index 会被推进到首个未到达航点。
bool pick_steer_target(const PathFollowAI& pf, float self_x, float self_y,
                       int& io_index, float& out_x, float& out_y) {
    const auto& path = pf.path;
    const int n = static_cast<int>(path.size());
    int idx = io_index;

    // 推进已到达航点
    while (idx < n) {
        float wx = PathfinderSystem::cell_center(path[idx].first);
        float wy = PathfinderSystem::cell_center(path[idx].second);
        float dx = wx - self_x, dy = wy - self_y;
        if (dx * dx + dy * dy < PATH_ARRIVE_R * PATH_ARRIVE_R) idx++;
        else break;
    }
    io_index = idx;
    if (idx >= n) return false;

    // 前瞻：在 lookahead 距离内，取沿途最远的可达航点（线段可走即安全，不会穿墙取点）
    int target = idx;
    for (int i = idx; i < n; i++) {
        float wx = PathfinderSystem::cell_center(path[i].first);
        float wy = PathfinderSystem::cell_center(path[i].second);
        float dx = wx - self_x, dy = wy - self_y;
        if (dx * dx + dy * dy > PATH_LOOKAHEAD * PATH_LOOKAHEAD) continue;
        if (PathfinderSystem::segment_walkable(self_x, self_y, wx, wy, true)) target = i;
    }
    out_x = PathfinderSystem::cell_center(path[target].first);
    out_y = PathfinderSystem::cell_center(path[target].second);
    return true;
}

// 巡逻选点：在当前位置附近（wander_radius 圆盘）随机采样下一个目标，
// 但候选点必须落在以 home 为圆心、leash_radius 为半径的领地内。
// 受控重试（上限 PATROL_PICK_TRIES）：全部被领地拒绝时回退到 home，
// 保证随机游走永不会把敌人带离出生区域（参考 Minecraft home_position_restriction）。
void pick_patrol_target(float self_x, float self_y, float home_x, float home_y,
                        float wander_radius, float leash_radius,
                        float& out_x, float& out_y) {
    const float leash2 = leash_radius * leash_radius;
    for (int i = 0; i < PATROL_PICK_TRIES; i++) {
        float angle = game_random::range(0.0f, std::numbers::pi_v<float> * 2.0f);
        float prob  = game_random::range(0.0f, 1.0f);
        float r = std::sqrt(prob) * wander_radius;
        float cx = self_x + std::cos(angle) * r;
        float cy = self_y + std::sin(angle) * r;
        float dx = cx - home_x, dy = cy - home_y;
        if (dx * dx + dy * dy <= leash2) {
            out_x = cx;
            out_y = cy;
            return;
        }
    }
    out_x = home_x;
    out_y = home_y;
}

}  // namespace

// =====================================================================
//  入口：查找玩家，分派到各行为 update 函数
// =====================================================================
void EnemyAISystem::update(World& world, float dt) {
    Entity player = INVALID_ENTITY;

    //绑定玩家
    for (auto e : world.query<Transform, IdentityTag>()) {
        auto* id = world.get_component<IdentityTag>(e);
        if (id && id->identity_type == Identity::Player) {
            player = e;
            break;
        }
    }

    update_patrol(world, dt);
    update_state_machine(world, dt, player);
}

// =====================================================================
//  随机巡逻行为：在出生点附近随机漫游，不主动追击玩家
//  实体需携带 EnemyAI + PatrolAI 且 ai_type == RandomPatrol。
//  Boss 也挂 PatrolAI（状态机 Patrol 态消费），但其巡逻由
//  update_state_machine 分派，这里按 ai_type 排除，避免双重驱动。
// =====================================================================
void EnemyAISystem::update_patrol(World& world, float dt) {
    for (auto e : world.query<Transform, Velocity, EnemyAI, PatrolAI>()) {
        auto* ai  = world.get_component<EnemyAI>(e);
        if (ai->ai_type != AIType::RandomPatrol) continue;

        auto* t   = world.get_component<Transform>(e);
        auto* v   = world.get_component<Velocity>(e);
        auto* pc  = world.get_component<PatrolAI>(e);
        auto* oc  = world.get_component<ObstacleContact>(e);

        // 冻结时停止一切行动
        auto* es = world.get_component<ElementStatus>(e);
        if (es && es->frozen.is_frozen) {
            v->vx = 0.0f;
            v->vy = 0.0f;
            continue;
        }

        update_patrol_behavior(dt, t, v, ai, pc, oc);

        // 末尾统一消费撞墙标志
        oc->blocked = false;
    }
}

// =====================================================================
//  状态机行为：Patrol（巡逻）→ Chase（追击）→ Flee（逃跑）
//  实体需同时携带 EnemyAI + StateMachineAI + PatrolAI + PathFollowAI（Boss 专用）
//
//  两阶段结构：
//    阶段1 decide_state_transition —— 集中决定状态转换（含 entry 动作），只改 state
//    阶段2 update_xxx_behavior     —— 各状态只算速度，不再改 state
//
//  与 PathfinderSystem 配合：Chase 态沿 A* 最短路径追玩家，遇墙自动绕路
//  撞墙（blocked）或玩家换 cell 或冷却归零 → 触发重算路径
// =====================================================================
void EnemyAISystem::update_state_machine(World& world, float dt, Entity player) {
    Transform* pt = (player != INVALID_ENTITY) ? world.get_component<Transform>(player) : nullptr;

    for (auto e : world.query<Transform, Velocity, EnemyAI, StateMachineAI, PatrolAI, PathFollowAI>()) {
        auto* t   = world.get_component<Transform>(e);
        auto* v   = world.get_component<Velocity>(e);
        auto* ai  = world.get_component<EnemyAI>(e);
        auto* sm  = world.get_component<StateMachineAI>(e);
        auto* pc  = world.get_component<PatrolAI>(e);
        auto* pf  = world.get_component<PathFollowAI>(e);
        auto* hp  = world.get_component<Health>(e);
        auto* oc  = world.get_component<ObstacleContact>(e);

        // 冻结时停止一切行动
        auto* es = world.get_component<ElementStatus>(e);
        if (es && es->frozen.is_frozen) {
            v->vx = 0.0f;
            v->vy = 0.0f;
            // 冻结期间也消费 blocked，避免积累
            if (oc->blocked) { oc->blocked = false; oc->block_push_x = 0; oc->block_push_y = 0; }
            continue;
        }

        // 注意：攻击冷却递减已由 CooldownSystem 统一完成，
        // 本系统不再处理 attack_timer（Cooldown 组件在 EnemyAI 之外独立递减）。

        // === 玩家相对距离 ===
        float dx = 0, dy = 0, dist = 0, dir_x = 0, dir_y = 0;
        if (pt) {
            dx = pt->x - t->x;
            dy = pt->y - t->y;
            dist = std::sqrt(dx * dx + dy * dy);
            float inv_d = (dist > 0.001f) ? 1.0f / dist : 0.0f;
            dir_x = dx * inv_d;
            dir_y = dy * inv_d;
        }

        // 阶段1：集中决定状态转换（含 entry 动作），可能改变 sm->state
        decide_state_transition(sm, ai, pc, pf, hp, pt, dist, dir_x, dir_y);

        // 阶段2：按当前状态执行行为（只算速度，不再改 state）
        // 每帧先重置临时倍率为 1.0，Flee 态再覆盖为 1.2，切走即自动恢复（无需各退出点写恢复）
        v->speed_mult = 1.0f;
        switch (sm->state) {
            case AIState::Patrol:
                update_patrol_behavior(dt, t, v, ai, pc, oc);
                break;
            case AIState::Chase:
                update_chase_behavior(dt, t, v, pf, pt, dir_x, dir_y, oc);
                break;
            case AIState::Flee:
                v->speed_mult = 1.2f;  // 逃跑 1.2 倍速：倍率走速度字段，方向保持归一化
                update_flee_behavior(v, dir_x, dir_y);
                break;
            default:
                sm->state = AIState::Patrol;
                v->vx = 0.0f;
                v->vy = 0.0f;
                break;
        }

        // 末尾统一消费撞墙标志
        oc->blocked = false;
    }
}

// =====================================================================
//  阶段1：集中决定状态转换（含 entry 动作）
//
//  判断顺序与原代码一致（保证行为等价）：
//    1. 无玩家       → Chase/Flee 回 Patrol
//    2. 血量低于阈值 → 任意态转 Flee（在有玩家前提下）
//    3. 各状态自身退出条件（此时玩家一定存在）：
//         · Patrol：dist < alert_radius        → Chase
//         · Chase ：dist > alert_radius * 1.5  → Patrol
//         · Flee  ：dist > alert_radius * 2.0  → Patrol
//
//  本函数只改 sm->state 与 entry 字段，不碰 Velocity（速度由行为阶段统一写入，
//  避免一帧被写两次）。
// =====================================================================
void EnemyAISystem::decide_state_transition(StateMachineAI* sm, const EnemyAI* ai, PatrolAI* pc,
                                            PathFollowAI* pf, const Health* hp, const Transform* pt,
                                            float dist, float dir_x, float dir_y) {
    // 1. 无玩家：Chase/Flee → Patrol；Patrol 正常继续
    if (!pt) {
        if (sm->state == AIState::Chase) {
            sm->state = AIState::Patrol;
            pf->path.clear();
            pf->path_index = 0;
            pc->patrol_timer = 0.0f;
        } else if (sm->state == AIState::Flee) {
            sm->state = AIState::Patrol;
            pc->patrol_timer = 0.0f;
        }
        return;  // 无玩家时不触发其他转换
    }

    // 2. 血量低 + 玩家在附近（警戒范围内）→ 逃跑（最高优先级）
    float hp_ratio = hp->max_hp > 0.0f ? (hp->current_hp / hp->max_hp) : 0.0f;
    if (hp_ratio < sm->flee_hp_ratio && sm->state != AIState::Flee && dist < ai->alert_radius) {
        sm->state = AIState::Flee;
        pf->path.clear();
        return;
    }

    // 3. 各状态自身退出条件（pt 一定存在）
    switch (sm->state) {
        case AIState::Patrol: {
            if (dist < ai->alert_radius) {
                // 发现玩家 → 直接切 Chase（不经过 Alert）
                sm->state = AIState::Chase;
                pf->repath_timer = 0.0f;   // 立即触发首次路径计算
                pf->heading_x = dir_x;    // 航向先对齐玩家，避免从巡逻方向缓慢转过来
                pf->heading_y = dir_y;
                pf->escape_timer = 0.0f;  // 清理上一次追击的脱困/卡死残留
                pf->stuck_timer  = 0.0f;
            }
            break;
        }

        case AIState::Chase: {
            // 逃出警戒区 1.5 倍 → 放弃追击回巡逻
            if (dist > ai->alert_radius * 1.5f) {
                sm->state = AIState::Patrol;
                pf->path.clear();
                pf->path_index = 0;
                pc->patrol_timer = 0.0f;   // 立刻在家园周围选新目标，不沿用 Chase 前的死目标
            }
            break;
        }

        case AIState::Flee: {
            // 跑远了回 Patrol
            if (dist > ai->alert_radius * 2.0f) {
                sm->state = AIState::Patrol;
                pc->patrol_timer = 0.0f;
            }
            break;
        }

        default:
            break;
    }
}

// =====================================================================
//  阶段1 巡逻核心（Boss Patrol 态与小怪随机巡逻共用）
//  只算速度，不改 state（发现玩家的转换在 decide_state_transition 处理）
//  数据统一在 PatrolAI 组件；撞墙避让沿推开方向反向选点，远离墙再随机
// =====================================================================
void EnemyAISystem::update_patrol_behavior(float dt, Transform* t, Velocity* v, EnemyAI* ai,
                                           PatrolAI* pc, ObstacleContact* oc) {
    // === 撞墙避让 ===
    if (oc->blocked) {
        float plen = std::sqrt(oc->block_push_x * oc->block_push_x +
                               oc->block_push_y * oc->block_push_y);
        if (plen > 0.001f) {
            float ux = oc->block_push_x / plen;
            float uy = oc->block_push_y / plen;
            pc->target_x = t->x + ux * PATROL_WALL_AVOID_DIST;
            pc->target_y = t->y + uy * PATROL_WALL_AVOID_DIST;
            pc->patrol_timer = pc->patrol_interval;  // 推开后走满一个周期再随机
        }
        oc->block_push_x = 0.0f;
        oc->block_push_y = 0.0f;
    }

    // 正常巡逻：到达目标 / 换点计时到期 → 在当前位置附近重选，受 leash 约束不漂离出生点
    pc->patrol_timer -= dt;
    float tdx = pc->target_x - t->x;
    float tdy = pc->target_y - t->y;
    float tdist = std::sqrt(tdx * tdx + tdy * tdy);

    // 硬牵绳兜底：已被推出领地（碰撞/击退等），直接朝 home 拉回，不等换点计时
    const float hdx = t->x - ai->home_x;
    const float hdy = t->y - ai->home_y;
    const bool outside_leash = (hdx * hdx + hdy * hdy) > pc->leash_radius * pc->leash_radius;

    if (outside_leash) {
        pc->target_x = ai->home_x;
        pc->target_y = ai->home_y;
        pc->patrol_timer = pc->patrol_interval;
    } else if (tdist < 8.0f || pc->patrol_timer <= 0.0f) {
        pick_patrol_target(t->x, t->y, ai->home_x, ai->home_y,
                           pc->patrol_radius, pc->leash_radius,
                           pc->target_x, pc->target_y);
        pc->patrol_timer = pc->patrol_interval;
    }

    // 朝（可能更新后的）目标点移动
    tdx = pc->target_x - t->x;
    tdy = pc->target_y - t->y;
    tdist = std::sqrt(tdx * tdx + tdy * tdy);
    if (tdist > 0.001f) {
        v->vx = tdx / tdist;
        v->vy = tdy / tdist;
    } else {
        v->vx = 0.0f;
        v->vy = 0.0f;
    }
}

// =====================================================================
//  阶段2 · Chase 行为：A* 寻路追击 + 卡死脱困 + 平滑转向
//  只算速度，不改 state（无玩家 / 逃出警戒区的转换已在 decide_state_transition 处理）
//  调用前提：玩家一定存在（pt 非空）
// =====================================================================
void EnemyAISystem::update_chase_behavior(float dt, Transform* t, Velocity* v,
                                          PathFollowAI* pf, const Transform* pt,
                                          float dir_x, float dir_y,
                                          ObstacleContact* oc) {
    // === 脱困态优先：卡死时锁定一个方向持续位移，必须先于正常寻路早退 ===
    if (pf->escape_timer > 0.0f) {
        pf->escape_timer -= dt;
        v->vx = pf->escape_x;
        v->vy = pf->escape_y;
        pf->heading_x = pf->escape_x;  // 同步航向，脱困结束后平滑衔接
        pf->heading_y = pf->escape_y;
        pf->stuck_timer = 0.0f;
        return;
    }

    // === 窗口式卡死检测 ===
    if (oc->blocked) {
        if (pf->stuck_timer <= 0.0f) {  // 刚进入被挡，记录窗口起点
            pf->stuck_origin_x = t->x;
            pf->stuck_origin_y = t->y;
        }
        pf->stuck_timer += dt;
    } else {
        pf->stuck_timer = 0.0f;
    }
    float net_dx = t->x - pf->stuck_origin_x;
    float net_dy = t->y - pf->stuck_origin_y;
    bool stuck = oc->blocked &&
                 pf->stuck_timer > PATH_STUCK_TIME &&
                 (net_dx * net_dx + net_dy * net_dy) < PATH_STUCK_DIST * PATH_STUCK_DIST;

    if (stuck) {
        float plen = std::sqrt(oc->block_push_x * oc->block_push_x +
                               oc->block_push_y * oc->block_push_y);
        if (plen > 0.001f) {
            pf->escape_x = oc->block_push_x / plen;
            pf->escape_y = oc->block_push_y / plen;
        } else {
            pf->escape_x = pf->heading_x;
            pf->escape_y = pf->heading_y;
        }
        float el = std::sqrt(pf->escape_x * pf->escape_x + pf->escape_y * pf->escape_y);
        if (el > 0.001f) { pf->escape_x /= el; pf->escape_y /= el; }
        else             { pf->escape_x = 1.0f;  pf->escape_y = 0.0f; }

        pf->escape_timer = PATH_ESCAPE_TIME;
        pf->stuck_timer  = 0.0f;
        pf->repath_timer = 0.0f;   // 脱困结束立即重算路径
        oc->block_push_x = 0.0f;
        oc->block_push_y = 0.0f;
        v->vx = pf->escape_x;
        v->vy = pf->escape_y;
        pf->heading_x = pf->escape_x;
        pf->heading_y = pf->escape_y;
        return;
    }

    // === A* 路径重算（节流到就定期重算，保证终点跟随玩家最新位置）===
    pf->repath_timer -= dt;
    if (pf->repath_timer <= 0.0f) {
        pf->repath_timer   = PATH_REPATH_CD;

        pf->path.clear();
        pf->path_index = 0;
        if (PathfinderSystem::find_path(t->x, t->y, pt->x, pt->y, pf->path)) {
            // 保留进度：吸附到离当前位置最近的、可达的航点
            int best = 0;
            float best_d2 = 1e18f;
            for (int i = 0; i < static_cast<int>(pf->path.size()); i++) {
                float wx = PathfinderSystem::cell_center(pf->path[i].first);
                float wy = PathfinderSystem::cell_center(pf->path[i].second);
                if (!PathfinderSystem::segment_walkable(t->x, t->y, wx, wy, true)) continue;
                float dx = wx - t->x, dy = wy - t->y;
                float d2 = dx * dx + dy * dy;
                if (d2 < best_d2) { best_d2 = d2; best = i; }
            }
            pf->path_index = best;
        }
        // 寻路失败：保持 path 为空，落到直奔玩家兜底，且受 CD 节流不再每帧死算
    }

    // === 选目标方向 ===
    float tx, ty;
    float desired_x, desired_y;
    if (pick_steer_target(*pf, t->x, t->y, pf->path_index, tx, ty)) {
        float dx = tx - t->x, dy = ty - t->y;
        float d = std::sqrt(dx * dx + dy * dy);
        if (d > 0.001f) { desired_x = dx / d; desired_y = dy / d; }
        else            { desired_x = dir_x;  desired_y = dir_y; }
    } else {
        desired_x = dir_x;  // 路径走完 / 无路径 → 直奔玩家
        desired_y = dir_y;
    }

    // === 贴墙轻量滑动 ===
    if (oc->blocked) {
        float plen = std::sqrt(oc->block_push_x * oc->block_push_x +
                               oc->block_push_y * oc->block_push_y);
        if (plen > 0.001f) {
            desired_x += (oc->block_push_x / plen) * PATH_PUSH_BASE;
            desired_y += (oc->block_push_y / plen) * PATH_PUSH_BASE;
        }
        oc->block_push_x = 0.0f;
        oc->block_push_y = 0.0f;
    }
    float dl = std::sqrt(desired_x * desired_x + desired_y * desired_y);
    if (dl > 0.001f) { desired_x /= dl; desired_y /= dl; }

    // === 平滑航向（帧率无关指数插值）→ 写速度 ===
    float blend = 1.0f - std::exp(-PATH_TURN_RATE * dt);
    pf->heading_x = pf->heading_x * (1.0f - blend) + desired_x * blend;
    pf->heading_y = pf->heading_y * (1.0f - blend) + desired_y * blend;
    float hl = std::sqrt(pf->heading_x * pf->heading_x + pf->heading_y * pf->heading_y);
    if (hl > 0.001f) {
        v->vx = pf->heading_x / hl;
        v->vy = pf->heading_y / hl;
    } else {
        v->vx = desired_x;
        v->vy = desired_y;
        pf->heading_x = desired_x;
        pf->heading_y = desired_y;
    }
}

// =====================================================================
//  阶段3 · Flee 行为：朝玩家反方向跑
//  只算速度，不改 state（无玩家 / 跑远回 Patrol 的转换已在 decide_state_transition 处理）
// =====================================================================
void EnemyAISystem::update_flee_behavior(Velocity* v, float dir_x, float dir_y) {
    // 朝玩家反方向跑（方向归一化；1.2 倍速由调用方写入 speed_mult）
    v->vx = -dir_x;
    v->vy = -dir_y;
}


