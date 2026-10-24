#include "CollisionSystem.h"
#include "../components/Transform.h"
#include "../components/SphereCollider.h"
#include "../components/BoxCollider.h"
#include "../components/IdentityTag.h"
#include "../components/ElementPayload.h"
#include "../components/ElementStatus.h"
#include "../components/ElementICDStatus.h"
#include "../components/AttackPower.h"
#include "../gameplay/BulletPool.h"
#include <cmath>

namespace {

// 圆 vs 轴对齐矩形（AABB）碰撞检测 + 推开向量计算
//   圆心 (cx, cy) 半径 r；矩形中心 (rx, ry) 半宽 hw 半高 hh
//   返回值：true=碰撞；若碰撞且 push_x/push_y 非空，则写入推开方向（单位向量 × 重叠量）
bool circle_rect_collide(float cx, float cy, float r,
                         float rx, float ry, float hw, float hh,
                         float& push_x, float& push_y) {
    // 矩形上离圆心最近的点（clamp 到矩形范围）
    float closest_x = cx;
    float closest_y = cy;
    if (closest_x < rx - hw) closest_x = rx - hw;
    else if (closest_x > rx + hw) closest_x = rx + hw;
    if (closest_y < ry - hh) closest_y = ry - hh;
    else if (closest_y > ry + hh) closest_y = ry + hh;

    float dx = cx - closest_x;
    float dy = cy - closest_y;
    float dist_sq = dx * dx + dy * dy;

    if (dist_sq > r * r) return false;

    // —— 计算推开向量 ——
    float dist = std::sqrt(dist_sq);
    if (dist > 0.0001f) {
        // 圆心在矩形外：沿圆心→最近点方向推开
        float overlap = r - dist;
        push_x = (dx / dist) * overlap;
        push_y = (dy / dist) * overlap;
    } else {
        // 圆心在矩形内部：选最近的一条边推开
        float left   = cx - (rx - hw);   // 到左边距离
        float right  = (rx + hw) - cx;   // 到右边距离
        float top    = cy - (ry - hh);   // 到上边距离
        float bottom = (ry + hh) - cy;   // 到下边距离
        float min_d = left;
        push_x = -1.0f; push_y = 0.0f;
        if (right < min_d)  { min_d = right;  push_x =  1.0f; push_y = 0.0f; }
        if (top < min_d)    { min_d = top;    push_x =  0.0f; push_y = -1.0f; }
        if (bottom < min_d) { min_d = bottom; push_x =  0.0f; push_y =  1.0f; }
        // 推开距离 = r + min_d（把圆心推出矩形外 r 距离）
        float push_len = r + min_d;
        push_x *= push_len;
        push_y *= push_len;
    }
    return true;
}

}

void CollisionSystem::update(World& world, float dt) {
    (void)dt;

    // ========== 收集三层实体 ==========
    std::vector<Entity> bullets;
    std::vector<Entity> enemies;
    std::vector<Entity> walls;

    for (auto e : world.query<Transform, IdentityTag>()) {
        auto* id = world.get_component<IdentityTag>(e);
        if (id->identity_type == Identity::Bullet)       bullets.push_back(e);
        else if (id->identity_type == Identity::Enemy)   enemies.push_back(e);
        else if (id->identity_type == Identity::Wall)    walls.push_back(e);
    }

    // ========== ① 可移动实体（玩家/敌人） vs 墙体：推开 ==========
    // 玩家和敌人都是球形 SphereCollider，墙体是 BoxCollider
    for (auto e : world.query<Transform, SphereCollider, IdentityTag>()) {
        auto* id = world.get_component<IdentityTag>(e);
        if (id->identity_type != Identity::Player &&
            id->identity_type != Identity::Enemy) continue;

        auto* t  = world.get_component<Transform>(e);
        auto* col = world.get_component<SphereCollider>(e);

        for (auto w : walls) {
            auto* wt = world.get_component<Transform>(w);
            auto* rc = world.get_component<BoxCollider>(w);
            if (!wt || !rc) continue;

            float px = 0.0f, py = 0.0f;
            if (circle_rect_collide(t->x, t->y, col->radius,
                                    wt->x, wt->y, rc->width * 0.5f, rc->height * 0.5f,
                                    px, py)) {
                t->x += px;
                t->y += py;
            }
        }
    }

    // ========== ② 子弹 vs 敌人（圆-圆，原有逻辑） ==========
    std::vector<Entity> consumed;
    consumed.reserve(bullets.size());

    for (auto b : bullets) {
        auto* bt      = world.get_component<Transform>(b);
        auto* bc      = world.get_component<SphereCollider>(b);
        auto* payload = world.get_component<ElementPayload>(b);
        auto* atk     = world.get_component<AttackPower>(b);
        bool  hit     = false;

        for (auto en : enemies) {
            auto* et  = world.get_component<Transform>(en);
            auto* ec  = world.get_component<SphereCollider>(en);
            float dx = bt->x - et->x;
            float dy = bt->y - et->y;
            float r  = bc->radius + ec->radius;

            if (dx*dx + dy*dy <= r * r) {
                auto* es  = world.get_component<ElementStatus>(en);
                auto* icd = world.get_component<ElementICDStatus>(en);
                if (payload && es) {
                    es->pending_base_damage = atk ? atk->base_damage : 0.0f;

                    bool allow = true;
                    if (icd) allow = ::icd_hit_allowed(*icd, payload->element, payload->strength);
                    if (allow) {
                        es->pending_element  = payload->element;
                        es->pending_strength = payload->strength;
                    }
                }
                hit = true;
                break;
            }
        }

        // 子弹未命中敌人 → 检测是否撞墙
        if (!hit) {
            for (auto w : walls) {
                auto* wt = world.get_component<Transform>(w);
                auto* rc = world.get_component<BoxCollider>(w);
                if (!wt || !rc) continue;

                float px = 0.0f, py = 0.0f;
                if (circle_rect_collide(bt->x, bt->y, bc->radius,
                                        wt->x, wt->y, rc->width * 0.5f, rc->height * 0.5f,
                                        px, py)) {
                    hit = true;
                    break;
                }
            }
        }

        if (hit) consumed.push_back(b);
    }

    // 命中的子弹（敌人 或 墙体）：归还对象池
    for (auto b : consumed) {
        release_bullet_entity(world, b);
    }
}
