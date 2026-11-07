#include "EnemyAISystem.h"
#include "../core/World.h"
#include "../components/Transform.h"
#include "../components/Velocity.h"
#include "../components/EnemyAI.h"
#include "../components/Health.h"
#include "../components/IdentityTag.h"
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
//  状态机行为：巡逻 → 警戒 → 战斗 → 逃跑
//  实体需同时携带 EnemyAI + StateMachineAI + Health
// =====================================================================
void EnemyAISystem::update_state_machine(World& world, float dt, Entity player) {
    Transform* pt = (player != INVALID_ENTITY) ? world.get_component<Transform>(player) : nullptr;

    for (auto e : world.query<Transform, Velocity, EnemyAI, StateMachineAI, Health>()) {
        auto* t   = world.get_component<Transform>(e);
        auto* v   = world.get_component<Velocity>(e);
        auto* ai  = world.get_component<EnemyAI>(e);
        auto* sm  = world.get_component<StateMachineAI>(e);
        auto* hp  = world.get_component<Health>(e);

        // 攻击冷却递减
        if (ai->attack_timer > 0.0f) ai->attack_timer -= dt;

        // 无玩家时停留在巡逻态
        if (!pt) {
            v->vx = 0.0f;
            v->vy = 0.0f;
            continue;
        }

        // 玩家相对距离
        float dx = pt->x - t->x;
        float dy = pt->y - t->y;
        float dist = std::sqrt(dx * dx + dy * dy);
        float dir_x = (dist > 0.001f) ? dx / dist : 0.0f;
        float dir_y = (dist > 0.001f) ? dy / dist : 0.0f;

        // 血量低 → 逃跑（最高优先级）
        float hp_ratio = hp->max_hp > 0.0f ? (hp->current_hp / hp->max_hp) : 0.0f;
        if (hp_ratio < sm->flee_hp_ratio && sm->state != AIState::Flee) {
            sm->state = AIState::Flee;
        }

        switch (sm->state) {
            case AIState::Patrol: {
                if (dist < ai->alert_radius) {
                    sm->state = AIState::Alert;
                } else {
                    sm->patrol_timer -= dt;
                    float tdx = sm->target_x - t->x;
                    float tdy = sm->target_y - t->y;
                    float tdist = std::sqrt(tdx * tdx + tdy * tdy);
                    if (tdist < 8.0f || sm->patrol_timer <= 0.0f) {
                        float angle = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 6.2831853f;
                        float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * sm->patrol_radius;
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

            case AIState::Alert: {
                v->vx = 0.0f;
                v->vy = 0.0f;
                if (dist < ai->attack_radius) {
                    sm->state = AIState::Combat;
                } else if (dist > ai->alert_radius * 1.5f) {
                    sm->state = AIState::Patrol;
                }
                break;
            }

            case AIState::Combat: {
                if (dist > ai->attack_radius * 1.5f) {
                    sm->state = AIState::Alert;
                } else if (dist > ai->attack_radius * 0.8f) {
                    v->vx = dir_x;
                    v->vy = dir_y;
                } else {
                    v->vx = 0.0f;
                    v->vy = 0.0f;
                }
                break;
            }

            case AIState::Flee: {
                v->vx = -dir_x * 1.2f;
                v->vy = -dir_y * 1.2f;
                if (dist > ai->alert_radius * 2.5f) {
                    sm->state = AIState::Patrol;
                }
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

        // 攻击冷却递减（接触伤害仍由 CollisionSystem 处理）
        if (ai->attack_timer > 0.0f) ai->attack_timer -= dt;

        rp->change_timer -= dt;
        float tdx = rp->target_x - t->x;
        float tdy = rp->target_y - t->y;
        float tdist = std::sqrt(tdx * tdx + tdy * tdy);

        // 到达目标点或超时 → 选新随机点
        if (tdist < 8.0f || rp->change_timer <= 0.0f) {
            float angle = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 6.2831853f;
            float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * rp->wander_radius;
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
