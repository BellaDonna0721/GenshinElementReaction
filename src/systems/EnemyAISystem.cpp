#include "EnemyAISystem.h"
#include "../core/World.h"
#include "../components/Transform.h"
#include "../components/Velocity.h"
#include "../components/EnemyAI.h"
#include "../components/Health.h"
#include "../components/IdentityTag.h"
#include <cmath>
#include <cstdlib>

void EnemyAISystem::update(World& world, float dt) {
    // 找到玩家实体
    Entity player = INVALID_ENTITY;
    for (auto e : world.query<Transform, IdentityTag>()) {
        auto* id = world.get_component<IdentityTag>(e);
        if (id && id->identity_type == Identity::Player) {
            player = e;
            break;
        }
    }
    if (player == INVALID_ENTITY) return;

    auto* pt = world.get_component<Transform>(player);
    if (!pt) return;

    for (auto e : world.query<Transform, Velocity, EnemyAI, Health>()) {
        auto* t  = world.get_component<Transform>(e);
        auto* v  = world.get_component<Velocity>(e);
        auto* ai = world.get_component<EnemyAI>(e);
        auto* hp = world.get_component<Health>(e);

        // 攻击冷却递减
        if (ai->attack_timer > 0.0f) ai->attack_timer -= dt;

        // 玩家相对距离
        float dx = pt->x - t->x;
        float dy = pt->y - t->y;
        float dist = std::sqrt(dx * dx + dy * dy);
        float dir_x = (dist > 0.001f) ? dx / dist : 0.0f;
        float dir_y = (dist > 0.001f) ? dy / dist : 0.0f;

        // ========== 状态转换判定 ==========
        float hp_ratio = hp->max_hp > 0.0f ? (hp->current_hp / hp->max_hp) : 0.0f;

        // 血量低 → 逃跑（最高优先级）
        if (hp_ratio < ai->flee_hp_ratio && ai->state != AIState::Flee) {
            ai->state = AIState::Flee;
        }

        switch (ai->state) {
            case AIState::Patrol: {
                if (dist < ai->alert_radius) {
                    ai->state = AIState::Alert;
                } else {
                    // 巡逻：朝目标点移动，到达或超时换新点
                    ai->patrol_timer -= dt;
                    float tdx = ai->target_x - t->x;
                    float tdy = ai->target_y - t->y;
                    float tdist = std::sqrt(tdx * tdx + tdy * tdy);
                    if (tdist < 8.0f || ai->patrol_timer <= 0.0f) {
                        // 新随机点：在 home 附近 patrol_radius 范围内
                        float angle = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 6.2831853f;
                        float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * ai->patrol_radius;
                        ai->target_x = ai->home_x + std::cos(angle) * r;
                        ai->target_y = ai->home_y + std::sin(angle) * r;
                        ai->patrol_timer = ai->patrol_interval;
                        tdx = ai->target_x - t->x;
                        tdy = ai->target_y - t->y;
                        tdist = std::sqrt(tdx * tdx + tdy * tdy);
                    }
                    if (tdist > 0.001f) {
                        // 只写单位方向，实际速度由 MovementSystem 乘以 ai->move_speed
                        v->vx = tdx / tdist;
                        v->vy = tdy / tdist;
                    } else {
                        v->vx = 0.0f;
                        v->vy = 0.0f;
                    }
                }
                break;
            }

            case AIState::Alert: {
                // 警戒：停止移动，面向玩家
                v->vx = 0.0f;
                v->vy = 0.0f;
                if (dist < ai->attack_radius) {
                    ai->state = AIState::Combat;
                } else if (dist > ai->alert_radius * 1.5f) {
                    ai->state = AIState::Patrol;
                }
                break;
            }

            case AIState::Combat: {
                if (dist > ai->attack_radius * 1.5f) {
                    // 玩家跑远 → 回到警戒
                    ai->state = AIState::Alert;
                } else if (dist > ai->attack_radius * 0.8f) {
                    // 追击玩家（单位方向，速度由 MovementSystem 处理）
                    v->vx = dir_x;
                    v->vy = dir_y;
                } else {
                    // 在攻击范围内：停下，等待攻击冷却
                    v->vx = 0.0f;
                    v->vy = 0.0f;
                    // 接触伤害由 CollisionSystem 处理
                }
                break;
            }

            case AIState::Flee: {
                // 逃跑：朝玩家反方向移动，1.2 倍速倍率
                v->vx = -dir_x * 1.2f;
                v->vy = -dir_y * 1.2f;
                if (dist > ai->alert_radius * 2.5f) {
                    // 逃远了 → 回到巡逻
                    ai->state = AIState::Patrol;
                }
                break;
            }
        }
    }
}
