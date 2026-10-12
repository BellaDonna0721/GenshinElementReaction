#include "DamageSystem.h"
#include "../components/Health.h"
#include "../components/Transform.h"
#include "../components/IdentityTag.h"
#include "../components/DamageQueue.h"
#include "../components/DamageNumberQueue.h"
#include "../gameplay/EnemyManager.h"
#include "../utils/element_color.h"
#include <vector>
#include <cmath>

void DamageSystem::update(World& world, float dt) {
    (void)dt;

    // 取出全局伤害队列（挂在 Entity 0 单例上）
    DamageQueue* queue = world.get_component<DamageQueue>(0);
    if (!queue || queue->events.empty()) return;

    DamageNumberQueue* num_queue = world.get_component<DamageNumberQueue>(0);

    // 本帧死亡的敌人（帧末统一销毁，避免遍历中删实体）
    std::vector<Entity> dead_enemies;

    for (const auto& ev : queue->events) {
        if (!world.is_alive(ev.target)) continue;

        Health* hp = world.get_component<Health>(ev.target);
        if (!hp) continue;

        // —— 扣血（预留：未来在此处查 Resistance 组件做抗性减免）——
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
            // 仅敌人走 EnemyManager 销毁流程；
            // 玩家死亡的游戏结束逻辑预留（未来在此触发）
            IdentityTag* id = world.get_component<IdentityTag>(ev.target);
            if (id && id->identity_type == Identity::Enemy) {
                dead_enemies.push_back(ev.target);
            }
        }
    }

    // 帧末统一销毁死亡敌人（先清空队列再销毁，避免事件中残留引用）
    queue->events.clear();

    for (Entity e : dead_enemies) {
        g_enemy_manager.on_enemy_died(world, e);
    }
}
