#include "CollisionSystem.h"
#include "../components/Transform.h"
#include "../components/Collider.h"
#include "../components/IdentityTag.h"
#include "../components/ElementPayload.h"
#include "../components/ElementStatus.h"
#include "../components/ElementICDStatus.h"
#include "../components/AttackPower.h"
#include "../gameplay/BulletPool.h"

void CollisionSystem::update(World& world, float dt) {
    (void)dt;

    // 先收集子弹层和敌人层（用 IdentityTag 在运行时过滤）
    std::vector<Entity> bullets;
    std::vector<Entity> enemies;

    for (auto e : world.query<Transform, Collider, IdentityTag>()) {
        auto* id = world.get_component<IdentityTag>(e);
        if (id->identity_type == Identity::Bullet)       bullets.push_back(e);
        else if (id->identity_type == Identity::Enemy)   enemies.push_back(e);
    }

    std::vector<Entity> consumed;
    consumed.reserve(bullets.size());

    for (auto b : bullets) {
        auto* bt      = world.get_component<Transform>(b);
        auto* bc      = world.get_component<Collider>(b);
        auto* payload = world.get_component<ElementPayload>(b);
        auto* atk     = world.get_component<AttackPower>(b);
        bool  hit     = false;

        for (auto en : enemies) {
            // enemies 已经用 Identity::Enemy 过滤了，无需再判断类型
            auto* et  = world.get_component<Transform>(en);
            auto* ec  = world.get_component<Collider>(en);
            float dx = bt->x - et->x;
            float dy = bt->y - et->y;
            // 碰撞半径完全用 Collider.radius 计算（与 RenderInfo.radius 解耦）
            float r  = bc->radius + ec->radius;

            if (dx*dx + dy*dy <= r * r) {
                auto* es  = world.get_component<ElementStatus>(en);
                auto* icd = world.get_component<ElementICDStatus>(en);
                if (payload && es) {
                    // 伤害与元素附着完全解耦：每次命中都记录基础伤害（不受 ICD 限制）
                    es->pending_base_damage = atk ? atk->base_damage : 0.0f;

                    // ——— ICD 判定：仅限制元素附着，不影响伤害 ———
                    //     通过才允许写入 pending_element；否则只造成直射伤害、不触发反应。
                    bool allow = true;
                    if (icd) allow = ::icd_hit_allowed(*icd, payload->element, payload->strength);
                    if (allow) {
                        es->pending_element  = payload->element;
                        es->pending_strength = payload->strength;
                    }
                }

                // 伤害结算统一交给 ElementReactionSystem：
                //   根据是否触发增幅反应，推送 base_damage 或 base_damage × mult，
                //   保证每次命中只产生一个伤害飘字（直射与增伤合并显示）。

                hit = true;
                break;
            }
        }

        if (hit) consumed.push_back(b);
    }

    // 命中的子弹：归还对象池
    for (auto b : consumed) {
        release_bullet_entity(world, b);
    }
}
