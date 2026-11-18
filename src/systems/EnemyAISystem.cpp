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

                // === A* 路径重算 ===
                sm->repath_timer -= dt;
                int self_cx = PathfinderSystem::world_to_cell(t->x);
                int self_cy = PathfinderSystem::world_to_cell(t->y);
                int tgt_cx  = PathfinderSystem::world_to_cell(pt->x);
                int tgt_cy  = PathfinderSystem::world_to_cell(pt->y);

                bool need_repath = blocked_consumed ||              // 撞墙了
                                   sm->repath_timer <= 0.0f ||    // 冷却归零兜底
                                   tgt_cx != sm->last_target_cx || // 玩家换 cell
                                   tgt_cy != sm->last_target_cy ||
                                   self_cx != sm->last_self_cx ||  // Boss 自身换 cell（可能被推开）
                                   self_cy != sm->last_self_cy ||
                                   sm->path.empty() ||             // 没路径
                                   sm->path_index >= static_cast<int>(sm->path.size()); // 路径走完了

                if (need_repath) {
                    sm->repath_timer = 0.3f;  // 重算冷却，沿墙滑动时响应更快
                    sm->last_target_cx = static_cast<short>(tgt_cx);
                    sm->last_target_cy = static_cast<short>(tgt_cy);
                    sm->last_self_cx   = static_cast<short>(self_cx);
                    sm->last_self_cy   = static_cast<short>(self_cy);
                    sm->path.clear();
                    sm->path_index = 0;

                    if (!PathfinderSystem::find_path(t->x, t->y, pt->x, pt->y, sm->path)) {
                        // 找不到路径（起点或终点完全被墙封死），直接朝玩家直走兜底
                        sm->path.clear();
                    }
                }

                // === 沿路径走 ===
                if (!sm->path.empty() && sm->path_index < static_cast<int>(sm->path.size())) {
                    auto [wcx, wcy] = sm->path[sm->path_index];
                    float wx = wcx * GameConfig::PATH_CELL_SIZE + GameConfig::PATH_CELL_SIZE * 0.5f;
                    float wy = wcy * GameConfig::PATH_CELL_SIZE + GameConfig::PATH_CELL_SIZE * 0.5f;
                    float wdx = wx - t->x;
                    float wdy = wy - t->y;
                    float wdist = std::sqrt(wdx * wdx + wdy * wdy);

                    // 到达阈值 12px：给 Boss 足够提前量转向，避免边角处刚到就切下一个导致抖动
                    if (wdist < 12.0f) {
                        // 到达当前 waypoint，切下一个
                        sm->path_index++;
                    } else {
                        // 朝 waypoint 方向移动
                        float inv_wd = 1.0f / wdist;
                        v->vx = wdx * inv_wd;
                        v->vy = wdy * inv_wd;
                    }
                } else {
                    // 路径走完了 / 找不到路径 → 直接朝玩家走
                    v->vx = dir_x;
                    v->vy = dir_y;
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
                if (dist > ai->alert_radius * 2.5f) {
                    sm->state = AIState::Patrol;
                    sm->patrol_timer = 0.0f;
                }
                break;
            }

            default: {
                // Alert 枚举保留兼容但不再切换，安全兜底回 Patrol
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
