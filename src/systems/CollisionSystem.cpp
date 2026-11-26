#include "CollisionSystem.h"
#include "../components/Transform.h"
#include "../components/Velocity.h"
#include "../components/SphereCollider.h"
#include "../components/BoxCollider.h"
#include "../components/IdentityTag.h"
#include "../components/ObstacleContact.h"
#include "../components/singletonComponents/HitEventQueue.h"
#include "../utils/GameConfig.h"
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
    std::vector<Entity> pickups;
    Entity player = INVALID_ENTITY;

    for (auto e : world.query<Transform, IdentityTag>()) {
        auto* id = world.get_component<IdentityTag>(e);
        if (id->identity_type == Identity::Bullet)       bullets.push_back(e);
        else if (id->identity_type == Identity::Enemy)   enemies.push_back(e);
        else if (id->identity_type == Identity::Wall)    walls.push_back(e);
        else if (id->identity_type == Identity::Pickup)  pickups.push_back(e);
        else if (id->identity_type == Identity::Player)  player = e;
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
                // 敌人撞墙：记录推开方向（单位法向），供 AI 下一帧反向避让。
                // 物理推到位移用带模长的 px/py（下方 t->x += px），此处只写方向，与模长解耦，
                // 保证与边界/敌人互撞来源量纲一致（均为单位法向累加）。
                if (id->identity_type == Identity::Enemy) {
                    auto* oc = world.get_component<ObstacleContact>(e);
                    if (oc) {
                        oc->blocked = true;
                        float plen = std::sqrt(px * px + py * py);
                        if (plen > 0.001f) {
                            oc->block_push_x += px / plen;
                            oc->block_push_y += py / plen;
                        }
                    }
                }

                t->x += px;
                t->y += py;

                // === 沿墙滑动：去掉 velocity 的墙法向分量，保留切向 ===
                // push 向量方向为"墙→实体外"，dot(v, n̂) < 0 表示 velocity 朝墙撞
                auto* v = world.get_component<Velocity>(e);
                if (v) {
                    float plen = std::sqrt(px * px + py * py);
                    if (plen > 0.001f) {
                        float nx = px / plen;
                        float ny = py / plen;
                        float dot = v->vx * nx + v->vy * ny;
                        if (dot < 0.0f) {  // dot<0：velocity 朝墙方向撞过来
                            v->vx -= dot * nx;
                            v->vy -= dot * ny;
                        }
                    }
                }
            }
        }
    }

    // ========== ② 敌人 vs 世界边界：贴边检测 → AI 反向 ==========
    // 注意：MovementSystem 已经做了基于 radius 的 clamp（先于 Collision 执行），
    // 所以到这里实体位置已经在边界上了，越界检测（t->y + pr > WORLD_H）永远不会命中。
    // 改成检测"贴边且朝边界方向移动"的状态，直接发 blocked 让 AI 下一帧反向。
    // 坐标不再重复推回（Movement 已保证位置正确）。
    for (auto en : enemies) {
        auto* t  = world.get_component<Transform>(en);
        auto* col = world.get_component<SphereCollider>(en);
        auto* oc = world.get_component<ObstacleContact>(en);
        auto* v  = world.get_component<Velocity>(en);
        if (!t || !col || !v) continue;
        if (!oc) continue;  // 只有挂了障碍反馈组件的敌人才处理撞墙避让

        float pr = col->radius;
        float eps = 0.1f;

        // === 沿边界滑动：贴边时去掉朝边界方向的 velocity 分量，保留切向 ===
        // 边界 push 向量方向为"边界→实体内"（远离边界），dot(v, n̂) < 0 表示朝边界撞
        auto mark_blocked_and_slide = [&](float nx, float ny) {
            oc->blocked = true; oc->block_push_x += nx; oc->block_push_y += ny;
            float dot = v->vx * nx + v->vy * ny;
            if (dot < 0.0f) {
                v->vx -= dot * nx;
                v->vy -= dot * ny;
            }
        };

        // 贴左边界且还在往左走
        if (t->x <= pr + eps && v->vx < 0) {
            mark_blocked_and_slide(1.0f, 0.0f);
        }
        // 贴右边界且还在往右走
        if (t->x >= GameConfig::WORLD_W - pr - eps && v->vx > 0) {
            mark_blocked_and_slide(-1.0f, 0.0f);
        }
        // 贴上边界且还在往上走（y 减小方向）
        if (t->y <= pr + eps && v->vy < 0) {
            mark_blocked_and_slide(0.0f, 1.0f);
        }
        // 贴下边界且还在往下走
        if (t->y >= GameConfig::WORLD_H - pr - eps && v->vy > 0) {
            mark_blocked_and_slide(0.0f, -1.0f);
        }
    }

    // ========== ③ 敌人 vs 敌人：互相推开 ==========
    // 圆-圆碰撞，重叠时沿连线方向各推开一半，避免敌人叠在一起
    for (size_t i = 0; i < enemies.size(); ++i) {
        auto* t1 = world.get_component<Transform>(enemies[i]);
        auto* c1 = world.get_component<SphereCollider>(enemies[i]);
        if (!t1 || !c1) continue;

        for (size_t j = i + 1; j < enemies.size(); ++j) {
            auto* t2 = world.get_component<Transform>(enemies[j]);
            auto* c2 = world.get_component<SphereCollider>(enemies[j]);
            if (!t2 || !c2) continue;

            float dx = t1->x - t2->x;
            float dy = t1->y - t2->y;
            float r  = c1->radius + c2->radius;
            if (dx * dx + dy * dy >= r * r) continue;

            float dist = std::sqrt(dx * dx + dy * dy);
            float ux, uy;
            if (dist > 0.0001f) {
                ux = dx / dist;
                uy = dy / dist;
            } else {
                ux = 1.0f; uy = 0.0f;  // 完全重叠：任意方向分开
            }
            float half = (r - dist) * 0.5f;
            t1->x += ux * half;
            t1->y += uy * half;
            t2->x -= ux * half;
            t2->y -= uy * half;

            // 记录阻挡标记，供 AI 反向避让（挂了障碍反馈组件的都写）
            auto* oc1 = world.get_component<ObstacleContact>(enemies[i]);
            if (oc1) { oc1->blocked = true; oc1->block_push_x += ux; oc1->block_push_y += uy; }
            auto* oc2 = world.get_component<ObstacleContact>(enemies[j]);
            if (oc2) { oc2->blocked = true; oc2->block_push_x -= ux; oc2->block_push_y -= uy; }
        }
    }

    // ========== ④ 子弹命中检测：只记录事件，不处理业务 ==========
    // 命中后 push 到 HitEventQueue，由 ElementReactionSystem（写附着）和
    // OutputSystem（生成特效 + 归池）顺序消费
    HitEventQueue* hit_queue = world.get_component<HitEventQueue>(0);
    if (!hit_queue) return;

    for (auto b : bullets) {
        auto* bt = world.get_component<Transform>(b);
        auto* bc = world.get_component<SphereCollider>(b);
        if (!bt || !bc) continue;

        HitEventQueue::HitEvent ev;
        ev.aggressor = b;
        ev.victim    = INVALID_ENTITY;
        ev.x         = bt->x;
        ev.y         = bt->y;
        bool hit = false;

        // 先检测命中敌人
        for (auto en : enemies) {
            auto* et = world.get_component<Transform>(en);
            auto* ec = world.get_component<SphereCollider>(en);
            if (!et || !ec) continue;
            float dx = bt->x - et->x;
            float dy = bt->y - et->y;
            float r  = bc->radius + ec->radius;
            if (dx*dx + dy*dy <= r * r) {
                ev.type   = HitEventQueue::Type::BulletHitEnemy;
                ev.victim = en;
                hit = true;
                break;  // 一颗子弹只命中第一个敌人
            }
        }

        // 未命中敌人 → 检测撞墙
        if (!hit) {
            for (auto w : walls) {
                auto* wt = world.get_component<Transform>(w);
                auto* rc = world.get_component<BoxCollider>(w);
                if (!wt || !rc) continue;
                float px = 0.0f, py = 0.0f;
                if (circle_rect_collide(bt->x, bt->y, bc->radius,
                                        wt->x, wt->y, rc->width * 0.5f, rc->height * 0.5f,
                                        px, py)) {
                    ev.type = HitEventQueue::Type::BulletHitWall;
                    hit = true;
                    break;
                }
            }
        }

        if (hit) hit_queue->events.push_back(ev);
    }

    // ========== ⑤ 敌人 vs 玩家：接触检测 → 写接触事件 ==========
    // 只记录几何接触事实，不做物理推开、不处理冷却/伤害；
    // 冷却判定与伤害由 DamageSystem 消费 EnemyTouchPlayer 事件完成。
    if (player != INVALID_ENTITY) {
        auto* pt = world.get_component<Transform>(player);
        auto* pc = world.get_component<SphereCollider>(player);
        if (pt && pc) {
            for (auto en : enemies) {
                auto* et = world.get_component<Transform>(en);
                auto* ec = world.get_component<SphereCollider>(en);
                if (!et || !ec) continue;
                float dx = pt->x - et->x;
                float dy = pt->y - et->y;
                float r  = pc->radius + ec->radius;
                if (dx * dx + dy * dy <= r * r) {
                    HitEventQueue::HitEvent cev;
                    cev.type      = HitEventQueue::Type::EnemyTouchPlayer;
                    cev.aggressor = en;
                    cev.victim    = player;
                    cev.x         = et->x;
                    cev.y         = et->y;
                    hit_queue->events.push_back(cev);
                }
            }

            // ⑥ 玩家 vs 道具：接触检测 → PickupSystem 消费
            for (auto pk : pickups) {
                auto* kt = world.get_component<Transform>(pk);
                auto* kc = world.get_component<SphereCollider>(pk);
                if (!kt || !kc) continue;
                float dx = pt->x - kt->x;
                float dy = pt->y - kt->y;
                float r  = pc->radius + kc->radius;
                if (dx * dx + dy * dy <= r * r) {
                    HitEventQueue::HitEvent pev;
                    pev.type      = HitEventQueue::Type::PlayerTouchPickup;
                    pev.aggressor = player;
                    pev.victim    = pk;
                    pev.x         = kt->x;
                    pev.y         = kt->y;
                    hit_queue->events.push_back(pev);
                }
            }
        }
    }
}
