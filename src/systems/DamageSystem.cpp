#include "DamageSystem.h"
#include "../components/Health.h"
#include "../components/Transform.h"
#include "../components/IdentityTag.h"
#include "../components/EnemyAI.h"
#include "../components/DamageQueue.h"
#include "../components/DamageNumberQueue.h"
#include "../components/GameState.h"
#include "../gameplay/EnemyManager.h"
#include "../utils/element_color.h"
#include <vector>
#include <cmath>

void DamageSystem::update(World& world, float dt) {
    // 取出全局伤害队列（挂在 Entity 0 单例上）
    DamageQueue* queue = world.get_component<DamageQueue>(0);
    if (!queue) return;

    DamageNumberQueue* num_queue = world.get_component<DamageNumberQueue>(0);
    GameState* gs = world.get_component<GameState>(0);

    // 本帧死亡的敌人（帧末统一销毁，避免遍历中删实体）
    std::vector<Entity> dead_enemies;

    // 先递减所有玩家的无敌帧（玩家通常只有一个）
    for (auto e : world.query<Health, IdentityTag>()) {
        auto* id = world.get_component<IdentityTag>(e);
        if (id && id->identity_type == Identity::Player) {
            Health* php = world.get_component<Health>(e);
            if (php && php->invincible_timer > 0.0f) {
                php->invincible_timer -= dt;
            }
        }
    }

    if (queue->events.empty()) return;

    for (const auto& ev : queue->events) {
        if (!world.is_alive(ev.target)) continue;

        Health* hp = world.get_component<Health>(ev.target);
        if (!hp) continue;

        // 玩家在无敌帧内，跳过此伤害
        IdentityTag* id = world.get_component<IdentityTag>(ev.target);
        if (id && id->identity_type == Identity::Player && hp->invincible_timer > 0.0f) {
            continue;
        }

        // 护盾优先抵挡（玩家）
        if (id && id->identity_type == Identity::Player && gs && gs->shield_count > 0) {
            gs->shield_count--;
            // 产生飘字提示护盾抵挡
            if (num_queue) {
                Transform* t = world.get_component<Transform>(ev.target);
                if (t) {
                    DamageNumber dn;
                    dn.x = t->x;
                    dn.y = t->y;
                    dn.amount = 0;  // 0 表示护盾抵挡
                    dn.r = 180; dn.g = 200; dn.b = 255;
                    num_queue->numbers.push_back(dn);
                }
            }
            continue;
        }

        // —— 扣血 ——
        hp->current_hp -= ev.amount;

        // —— 产生伤害飘字（元素色 + 整数）——
        Transform* t = world.get_component<Transform>(ev.target);
        if (num_queue && t) {
            ElementColor c = get_element_color(ev.element);
            DamageNumber dn;
            dn.x = t->x;
            dn.y = t->y;
            dn.amount = static_cast<int>(std::round(ev.amount));
            dn.r = c.r;
            dn.g = c.g;
            dn.b = c.b;
            num_queue->numbers.push_back(dn);
        }

        // —— 死亡判定 ——
        if (hp->current_hp <= 0.0f) {
            if (id) {
                if (id->identity_type == Identity::Enemy) {
                    dead_enemies.push_back(ev.target);
                } else if (id->identity_type == Identity::Player) {
                    // 玩家死亡 → GameOver
                    if (gs) gs->phase = GamePhase::GameOver;
                }
            }
        }
    }

    // 帧末统一销毁死亡敌人
    queue->events.clear();

    for (Entity e : dead_enemies) {
        // 击杀加分
        if (gs) {
            EnemyAI* ai = world.get_component<EnemyAI>(e);
            if (ai) gs->score += ai->score_value;
        }
        g_enemy_manager.on_enemy_died(world, e);
    }
}
