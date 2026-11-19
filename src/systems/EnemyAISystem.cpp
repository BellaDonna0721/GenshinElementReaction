#include "EnemyAISystem.h"
#include "../core/World.h"
#include "../components/Transform.h"
#include "../components/Velocity.h"
#include "../components/EnemyAI.h"
#include "../components/Health.h"
#include "../components/IdentityTag.h"
#include "../components/ElementStatus.h"
#include "../utils/GameConfig.h"
#include "PathfinderSystem.h"
#include <cmath>
#include <cstdlib>

namespace {

// ===== Chase 路径跟随参数 =====
constexpr float PATH_REPATH_CD   = 0.25f;  // 重算最小间隔（秒），节流避免贴墙每帧抖动
constexpr float PATH_STUCK_TIME  = 0.4f;   // 持续被挡多久后开始判定卡死
constexpr float PATH_STUCK_DIST  = 30.0f;  // 卡死窗口内净位移小于此值才算真卡死（px）
constexpr float PATH_ESCAPE_TIME = 0.6f;   // 脱困态锁定方向的持续时长（秒）
constexpr float PATH_LOOKAHEAD   = 80.0f;  // 前瞻距离：选该距离内最远的可达航点
constexpr float PATH_ARRIVE_R    = 18.0f;  // 航点到达半径
constexpr float PATH_TURN_RATE   = 8.0f;   // 转向平滑速率（/秒，指数插值）
constexpr float PATH_PUSH_BASE   = 1.2f;   // 正常贴墙时推开方向揉入转向的权重（轻量滑动）

// 从 io_index 开始推进已到达航点，并在 lookahead 内选最远的、从当前位置可达的航点。
// 可达判定用 segment_walkable（世界坐标采样），即使 Boss 本体压在膨胀格上也能正确取点。
// 返回 false 表示路径已走完（调用方应直奔玩家）。io_index 会被推进到首个未到达航点。
bool pick_steer_target(const StateMachineAI& sm, float self_x, float self_y,
                       int& io_index, float& out_x, float& out_y) {
    const auto& path = sm.path;
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

}  // namespace

// =====================================================================
//  入口：查找玩家，分派到各行为 update 函数
// =====================================================================
void EnemyAISystem::update(World& world, float dt) {
    Entity player = INVALID_ENTITY;
    for (auto e : world.query<Transform, IdentityTag>()) {
        auto* id = world.get_component<IdentityTag>(e);
        if (id && id->identity_type == Identity::Player) {
            player = e;
            break;
        }
    }

    update_state_machine(world, dt, player);
    update_random_patrol(world, dt);
}

// =====================================================================
//  状态机行为：Patrol（巡逻）→ Chase（追击）→ Flee（逃跑）
//  实体需同时携带 EnemyAI + StateMachineAI + Health（Boss 专用）
//
//  与 PathfinderSystem 配合：Chase 态沿 A* 最短路径追玩家，遇墙自动绕路
//  撞墙（blocked）或玩家换 cell 或冷却归零 → 触发重算路径
// =====================================================================
void EnemyAISystem::update_state_machine(World& world, float dt, Entity player) {
    Transform* pt = (player != INVALID_ENTITY) ? world.get_component<Transform>(player) : nullptr;

    for (auto e : world.query<Transform, Velocity, EnemyAI, StateMachineAI, Health>()) {
        auto* t   = world.get_component<Transform>(e);
        auto* v   = world.get_component<Velocity>(e);
        auto* ai  = world.get_component<EnemyAI>(e);
        auto* sm  = world.get_component<StateMachineAI>(e);
        auto* hp  = world.get_component<Health>(e);

        // 冻结时停止一切行动
        auto* es = world.get_component<ElementStatus>(e);
        if (es && es->frozen.is_frozen) {
            v->vx = 0.0f;
            v->vy = 0.0f;
            // 冻结期间也消费 blocked，避免积累
            if (sm->blocked) { sm->blocked = false; sm->block_push_x = 0; sm->block_push_y = 0; }
            continue;
        }

        // 攻击冷却递减
        if (ai->attack_timer > 0.0f) ai->attack_timer -= dt;

        // === 撞墙避让 ===
        // CollisionSystem 写 blocked 时也会写给 StateMachineAI（Boss），
        // 这里只消费 blocked 标志位；block_push_x/y 留给各状态分支按需读取后自行清零
        bool blocked_consumed = sm->blocked;
        if (sm->blocked) {
            sm->blocked = false;
            // block_push_x/y 暂不清零，Patrol 分支读取后再清
        }

        // 无玩家：Chase/Flee → Patrol；Patrol 正常继续
        if (!pt) {
            if (sm->state == AIState::Chase) {
                sm->state = AIState::Patrol;
                sm->path.clear();
                sm->path_index = 0;
                sm->patrol_timer = 0.0f;
            } else if (sm->state == AIState::Flee) {
                sm->state = AIState::Patrol;
                sm->patrol_timer = 0.0f;
            }
        }

        // === 血量低 → 逃跑（最高优先级） ===
        float hp_ratio = hp->max_hp > 0.0f ? (hp->current_hp / hp->max_hp) : 0.0f;
        if (hp_ratio < sm->flee_hp_ratio && sm->state != AIState::Flee) {
            sm->state = AIState::Flee;
            sm->path.clear();
        }

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

        switch (sm->state) {
            case AIState::Patrol: {
                if (pt && dist < ai->alert_radius) {
                    // 发现玩家 → 直接切 Chase（不经过 Alert）
                    sm->state = AIState::Chase;
                    sm->repath_timer = 0.0f;  // 立即触发首次路径计算
                    sm->heading_x = dir_x;    // 航向先对齐玩家，避免从巡逻方向缓慢转过来
                    sm->heading_y = dir_y;
                    sm->escape_timer = 0.0f;  // 清理上一次追击的脱困/卡死残留
                    sm->stuck_timer  = 0.0f;
                } else {
                    // === 撞墙避让（与 RandomPatrolAI 同口径） ===
                    // Patrol 态自身没路径规划，撞墙时改目标到推开方向，远离墙再随机选点
                    if (blocked_consumed) {
                        float plen = std::sqrt(sm->block_push_x * sm->block_push_x +
                                               sm->block_push_y * sm->block_push_y);
                        if (plen > 0.001f) {
                            float ux = sm->block_push_x / plen;
                            float uy = sm->block_push_y / plen;
                            sm->target_x = t->x + ux * 150.0f;
                            sm->target_y = t->y + uy * 150.0f;
                            sm->patrol_timer = sm->patrol_interval;  // 推开后走满一个周期再随机
                        }
                        sm->block_push_x = 0.0f;
                        sm->block_push_y = 0.0f;
                    }

                    // 正常巡逻
                    sm->patrol_timer -= dt;
                    float tdx = sm->target_x - t->x;
                    float tdy = sm->target_y - t->y;
                    float tdist = std::sqrt(tdx * tdx + tdy * tdy);
                    if (tdist < 8.0f || sm->patrol_timer <= 0.0f) {
                        float angle = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 6.2831853f;
                        float probability = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                        float r = std::sqrt(probability) * sm->patrol_radius;
                        sm->target_x = ai->home_x + std::cos(angle) * r;
                        sm->target_y = ai->home_y + std::sin(angle) * r;
                        sm->patrol_timer = sm->patrol_interval;
                        tdx = sm->target_x - t->x;
                        tdy = sm->target_y - t->y;
                        tdist = std::sqrt(tdx * tdx + tdy * tdy);
                    }
                    if (tdist > 0.001f) {
                        v->vx = tdx / tdist;
                        v->vy = tdy / tdist;
                    } else {
                        v->vx = 0.0f;
                        v->vy = 0.0f;
                    }
                }
                break;
            }

            case AIState::Chase: {
                if (!pt) {
                    sm->state = AIState::Patrol;
                    sm->patrol_timer = 0.0f;   // 立刻选新目标
                    v->vx = 0.0f; v->vy = 0.0f; break;
                }

                // 逃出警戒区 1.5 倍 → 放弃追击回巡逻
                if (dist > ai->alert_radius * 1.5f) {
                    sm->state = AIState::Patrol;
                    sm->path.clear();
                    sm->path_index = 0;
                    sm->patrol_timer = 0.0f;   // 立刻在家园周围选新目标，不沿用 Chase 前的死目标
                    break;
                }

                // === 脱困态优先：卡死时锁定一个方向持续位移，必须先于正常寻路早退 ===
                // 教训：只把瞬时推开方向揉进转向，方向会每帧来回翻 → 原地左右晃；
                //       必须进入独立脱困态、锁定方向直到脱离，才不会反复犹豫。
                if (sm->escape_timer > 0.0f) {
                    sm->escape_timer -= dt;
                    v->vx = sm->escape_x;
                    v->vy = sm->escape_y;
                    sm->heading_x = sm->escape_x;  // 同步航向，脱困结束后平滑衔接
                    sm->heading_y = sm->escape_y;
                    sm->stuck_timer = 0.0f;
                    break;
                }

                // === 窗口式卡死检测 ===
                // 持续被挡 且 STUCK_TIME 内净位移 < STUCK_DIST 才算真卡死。
                // 用窗口净位移而非单帧位移：左右晃动时单帧都在动，但净位移≈0，能正确识别。
                if (blocked_consumed) {
                    if (sm->stuck_timer <= 0.0f) {  // 刚进入被挡，记录窗口起点
                        sm->stuck_origin_x = t->x;
                        sm->stuck_origin_y = t->y;
                    }
                    sm->stuck_timer += dt;
                } else {
                    sm->stuck_timer = 0.0f;
                }
                float net_dx = t->x - sm->stuck_origin_x;
                float net_dy = t->y - sm->stuck_origin_y;
                bool stuck = blocked_consumed &&
                             sm->stuck_timer > PATH_STUCK_TIME &&
                             (net_dx * net_dx + net_dy * net_dy) < PATH_STUCK_DIST * PATH_STUCK_DIST;

                if (stuck) {
                    // 锁定脱困方向：优先沿碰撞推开方向（凹角口袋中推开方向指向开阔地），
                    // 没有推开信息时沿当前朝向走，保证一定产生位移。
                    float plen = std::sqrt(sm->block_push_x * sm->block_push_x +
                                           sm->block_push_y * sm->block_push_y);
                    if (plen > 0.001f) {
                        sm->escape_x = sm->block_push_x / plen;
                        sm->escape_y = sm->block_push_y / plen;
                    } else {
                        sm->escape_x = sm->heading_x;
                        sm->escape_y = sm->heading_y;
                    }
                    float el = std::sqrt(sm->escape_x * sm->escape_x + sm->escape_y * sm->escape_y);
                    if (el > 0.001f) { sm->escape_x /= el; sm->escape_y /= el; }
                    else             { sm->escape_x = 1.0f;  sm->escape_y = 0.0f; }

                    sm->escape_timer = PATH_ESCAPE_TIME;
                    sm->stuck_timer  = 0.0f;
                    sm->repath_timer = 0.0f;   // 脱困结束立即重算路径
                    sm->block_push_x = 0.0f;
                    sm->block_push_y = 0.0f;
                    // 本帧直接按脱困方向走（不经平滑，保证立刻位移）
                    v->vx = sm->escape_x;
                    v->vy = sm->escape_y;
                    sm->heading_x = sm->escape_x;
                    sm->heading_y = sm->escape_y;
                    break;
                }

                // === A* 路径重算（节流到就定期重算，保证终点跟随玩家最新位置）===
                // 触发：节流 CD 归零即重算。原逻辑需"玩家换 cell 或路径走完"才重算，
                // 但玩家在同一 cell 内移动时路径终点卡在 cell 中心，用户看不到更新。
                sm->repath_timer -= dt;
                int tgt_cx = PathfinderSystem::world_to_cell(pt->x);
                int tgt_cy = PathfinderSystem::world_to_cell(pt->y);

                if (sm->repath_timer <= 0.0f) {
                    sm->repath_timer   = PATH_REPATH_CD;
                    sm->last_target_cx = static_cast<short>(tgt_cx);
                    sm->last_target_cy = static_cast<short>(tgt_cy);

                    sm->path.clear();
                    sm->path_index = 0;
                    if (PathfinderSystem::find_path(t->x, t->y, pt->x, pt->y, sm->path)) {
                        // 保留进度：吸附到离当前位置最近的、可达的航点，
                        // 避免重算后 path_index 归零导致 Boss 回头走已走过的路。
                        int best = 0;
                        float best_d2 = 1e18f;
                        for (int i = 0; i < static_cast<int>(sm->path.size()); i++) {
                            float wx = PathfinderSystem::cell_center(sm->path[i].first);
                            float wy = PathfinderSystem::cell_center(sm->path[i].second);
                            if (!PathfinderSystem::segment_walkable(t->x, t->y, wx, wy, true)) continue;
                            float dx = wx - t->x, dy = wy - t->y;
                            float d2 = dx * dx + dy * dy;
                            if (d2 < best_d2) { best_d2 = d2; best = i; }
                        }
                        sm->path_index = best;
                    }
                    // 寻路失败：保持 path 为空，落到直奔玩家兜底，且受 CD 节流不再每帧死算
                }

                // === 选目标方向 ===
                float tx, ty;
                float desired_x, desired_y;
                if (pick_steer_target(*sm, t->x, t->y, sm->path_index, tx, ty)) {
                    float dx = tx - t->x, dy = ty - t->y;
                    float d = std::sqrt(dx * dx + dy * dy);
                    if (d > 0.001f) { desired_x = dx / d; desired_y = dy / d; }
                    else            { desired_x = dir_x;  desired_y = dir_y; }
                } else {
                    desired_x = dir_x;  // 路径走完 / 无路径 → 直奔玩家
                    desired_y = dir_y;
                }

                // === 贴墙轻量滑动：被挡时把推开方向揉进目标方向，拐角不硬切 ===
                // （真正卡死由脱困态处理，这里只做日常沿墙滑行。）
                if (blocked_consumed) {
                    float plen = std::sqrt(sm->block_push_x * sm->block_push_x +
                                           sm->block_push_y * sm->block_push_y);
                    if (plen > 0.001f) {
                        desired_x += (sm->block_push_x / plen) * PATH_PUSH_BASE;
                        desired_y += (sm->block_push_y / plen) * PATH_PUSH_BASE;
                    }
                    sm->block_push_x = 0.0f;  // Chase 消费完清零，避免跨帧累积
                    sm->block_push_y = 0.0f;
                }
                float dl = std::sqrt(desired_x * desired_x + desired_y * desired_y);
                if (dl > 0.001f) { desired_x /= dl; desired_y /= dl; }

                // === 平滑航向（帧率无关指数插值）→ 写速度 ===
                // heading 独立存储，不读 Velocity，避免被 CollisionSystem 的投影污染。
                float blend = 1.0f - std::exp(-PATH_TURN_RATE * dt);
                sm->heading_x = sm->heading_x * (1.0f - blend) + desired_x * blend;
                sm->heading_y = sm->heading_y * (1.0f - blend) + desired_y * blend;
                float hl = std::sqrt(sm->heading_x * sm->heading_x + sm->heading_y * sm->heading_y);
                if (hl > 0.001f) {
                    v->vx = sm->heading_x / hl;
                    v->vy = sm->heading_y / hl;
                } else {
                    v->vx = desired_x;
                    v->vy = desired_y;
                    sm->heading_x = desired_x;
                    sm->heading_y = desired_y;
                }
                break;
            }

            case AIState::Flee: {
                if (!pt) {
                    sm->state = AIState::Patrol;
                    sm->path.clear();
                    sm->patrol_timer = 0.0f;
                    v->vx = 0.0f; v->vy = 0.0f;
                    break;
                }
                // 朝玩家反方向跑
                v->vx = -dir_x * 1.2f;
                v->vy = -dir_y * 1.2f;
                // 跑远了回 Patrol
                if (dist > ai->alert_radius * 2.0f) {
                    sm->state = AIState::Patrol;
                    sm->patrol_timer = 0.0f;
                }
                break;
            }

            default: {
                sm->state = AIState::Patrol;
                v->vx = 0.0f;
                v->vy = 0.0f;
                break;
            }
        }
    }
}

// =====================================================================
//  随机巡逻行为：在出生点附近随机漫游，不主动追击玩家
//  实体需同时携带 EnemyAI + RandomPatrolAI
// =====================================================================
void EnemyAISystem::update_random_patrol(World& world, float dt) {
    for (auto e : world.query<Transform, Velocity, EnemyAI, RandomPatrolAI>()) {
        auto* t   = world.get_component<Transform>(e);
        auto* v   = world.get_component<Velocity>(e);
        auto* ai  = world.get_component<EnemyAI>(e);
        auto* rp  = world.get_component<RandomPatrolAI>(e);

        // 冻结时停止一切行动
        auto* es = world.get_component<ElementStatus>(e);
        if (es && es->frozen.is_frozen) {
            v->vx = 0.0f;
            v->vy = 0.0f;
            continue;
        }

        // 攻击冷却递减（接触伤害仍由 CollisionSystem 处理）
        if (ai->attack_timer > 0.0f) ai->attack_timer -= dt;

        // 撞障碍避让：沿推开方向（障碍→自身）反向走一段，远离墙/敌人
        if (rp->blocked) {
            rp->blocked = false;
            float plen = std::sqrt(rp->block_push_x * rp->block_push_x +
                                   rp->block_push_y * rp->block_push_y);
            if (plen > 0.001f) {
                float ux = rp->block_push_x / plen;
                float uy = rp->block_push_y / plen;
                rp->target_x = t->x + ux * 120.0f;
                rp->target_y = t->y + uy * 120.0f;
            }
            rp->block_push_x = 0.0f;
            rp->block_push_y = 0.0f;
            rp->change_timer = rp->change_interval;  // 反向后走满一个周期再随机
        }

        rp->change_timer -= dt;
        float tdx = rp->target_x - t->x;
        float tdy = rp->target_y - t->y;
        float tdist = std::sqrt(tdx * tdx + tdy * tdy);

        // 到达目标点或超时 → 选新随机点
        if (tdist < 8.0f || rp->change_timer <= 0.0f) {
            float angle = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 6.2831853f;
            float probability = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
            float r = std::sqrt(probability) * rp->wander_radius;

            rp->target_x = ai->home_x + std::cos(angle) * r;
            rp->target_y = ai->home_y + std::sin(angle) * r;
            rp->change_timer = rp->change_interval;
            tdx = rp->target_x - t->x;
            tdy = rp->target_y - t->y;
            tdist = std::sqrt(tdx * tdx + tdy * tdy);
        }

        if (tdist > 0.001f) {
            v->vx = tdx / tdist;
            v->vy = tdy / tdist;
        } else {
            v->vx = 0.0f;
            v->vy = 0.0f;
        }
    }
}
