#include "CollisionSystem.h"
#include "../components/Transform.h"
#include "../components/Velocity.h"
#include "../components/SphereCollider.h"
#include "../components/BoxCollider.h"
#include "../components/IdentityTag.h"
#include "../components/ElementPayload.h"
#include "../components/ElementStatus.h"
#include "../components/ElementICDStatus.h"
#include "../components/EnemyAI.h"
#include "../components/Health.h"
#include "../components/RenderInfo.h"
#include "../components/singletonComponents/DamageQueue.h"
#include "../components/singletonComponents/HitFxQueue.h"
#include "../utils/element_color.h"
#include "../utils/GameConfig.h"
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
    Entity player = INVALID_ENTITY;

    HitFxQueue* fx_queue = world.get_component<HitFxQueue>(0);
    // 帧首清空上一帧命中特效（wasm_api 直接读此 vector，由写入方管理生命周期）
    if (fx_queue) fx_queue->items.clear();

    for (auto e : world.query<Transform, IdentityTag>()) {
        auto* id = world.get_component<IdentityTag>(e);
        if (id->identity_type == Identity::Bullet)       bullets.push_back(e);
        else if (id->identity_type == Identity::Enemy)   enemies.push_back(e);
        else if (id->identity_type == Identity::Wall)    walls.push_back(e);
        else if (id->identity_type == Identity::Player)  player = e;
    }

    // ========== ① 敌人 vs 玩家：接触伤害 ==========
    if (player != INVALID_ENTITY) {
        auto* pt  = world.get_component<Transform>(player);
        auto* pc  = world.get_component<SphereCollider>(player);
        auto* php = world.get_component<Health>(player);
        DamageQueue* dq = world.get_component<DamageQueue>(0);

        if (pt && pc && php && dq && php->invincible_timer <= 0.0f) {
            for (auto en : enemies) {
                auto* et  = world.get_component<Transform>(en);
                auto* ec  = world.get_component<SphereCollider>(en);
                auto* eai = world.get_component<EnemyAI>(en);
                if (!et || !ec || !eai) continue;

                float dx = pt->x - et->x;
                float dy = pt->y - et->y;
                float r  = pc->radius + ec->radius;
                if (dx * dx + dy * dy <= r * r && eai->attack_timer <= 0.0f) {
                    // 接触伤害
                    DamageQueue::DamageEvent ev;
                    ev.target   = player;
                    ev.source   = en;  // 来源：发起接触的敌人
                    ev.amount   = eai->attack_power;
                    ev.element  = ElementType::None;
                    ev.reaction = ReactionType::None;
                    dq->events.push_back(ev);

                    // 重置攻击冷却（无敌帧由 DamageSystem 扣血后统一设置，避免同帧误跳过）
                    eai->attack_timer = eai->attack_cooldown;
                    break;  // 一帧只受一次接触伤害
                }
            }
        }
    }

    // ========== ② 可移动实体（玩家/敌人） vs 墙体：推开 ==========
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
                // 敌人撞墙：记录推开方向，供 AI 下一帧反向避让
                // StateMachineAI（Boss）和 RandomPatrolAI（小怪）都写
                if (id->identity_type == Identity::Enemy) {
                    auto* rp = world.get_component<RandomPatrolAI>(e);
                    auto* sm = world.get_component<StateMachineAI>(e);
                    if (rp) { rp->blocked = true; rp->block_push_x += px; rp->block_push_y += py; }
                    if (sm) { sm->blocked = true; sm->block_push_x += px; sm->block_push_y += py; }
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

    // ========== ③ 敌人 vs 世界边界：贴边检测 → AI 反向 ==========
    // 注意：MovementSystem 已经做了基于 radius 的 clamp（先于 Collision 执行），
    // 所以到这里实体位置已经在边界上了，越界检测（t->y + pr > WORLD_H）永远不会命中。
    // 改成检测"贴边且朝边界方向移动"的状态，直接发 blocked 让 AI 下一帧反向。
    // 坐标不再重复推回（Movement 已保证位置正确）。
    for (auto en : enemies) {
        auto* t  = world.get_component<Transform>(en);
        auto* col = world.get_component<SphereCollider>(en);
        auto* rp = world.get_component<RandomPatrolAI>(en);
        auto* sm = world.get_component<StateMachineAI>(en);
        auto* v  = world.get_component<Velocity>(en);
        if (!t || !col || !v) continue;
        if (!rp && !sm) continue;  // 只有 Boss 或小怪才处理撞墙避让

        float pr = col->radius;
        float eps = 0.1f;

        // === 沿边界滑动：贴边时去掉朝边界方向的 velocity 分量，保留切向 ===
        // 边界 push 向量方向为"边界→实体内"（远离边界），dot(v, n̂) < 0 表示朝边界撞
        auto mark_blocked_and_slide = [&](float nx, float ny) {
            if (rp) { rp->blocked = true; rp->block_push_x += nx; rp->block_push_y += ny; }
            if (sm) { sm->blocked = true; sm->block_push_x += nx; sm->block_push_y += ny; }
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

    // ========== 敌人 vs 敌人：互相推开 ==========
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

            // 记录阻挡标记，供 AI 反向避让（Boss 和小怪都写）
            auto* rp1 = world.get_component<RandomPatrolAI>(enemies[i]);
            auto* sm1 = world.get_component<StateMachineAI>(enemies[i]);
            if (rp1) { rp1->blocked = true; rp1->block_push_x += ux; rp1->block_push_y += uy; }
            if (sm1) { sm1->blocked = true; sm1->block_push_x += ux; sm1->block_push_y += uy; }
            auto* rp2 = world.get_component<RandomPatrolAI>(enemies[j]);
            auto* sm2 = world.get_component<StateMachineAI>(enemies[j]);
            if (rp2) { rp2->blocked = true; rp2->block_push_x -= ux; rp2->block_push_y -= uy; }
            if (sm2) { sm2->blocked = true; sm2->block_push_x -= ux; sm2->block_push_y -= uy; }
        }
    }

    // ========== ② 子弹 vs 敌人（圆-圆，原有逻辑） ==========
    std::vector<Entity> consumed;
    consumed.reserve(bullets.size());

    for (auto b : bullets) {
        auto* bt      = world.get_component<Transform>(b);
        auto* bc      = world.get_component<SphereCollider>(b);
        auto* payload = world.get_component<ElementPayload>(b);
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
                    es->pending_base_damage = payload->base_damage;

                    bool allow = true;
                    if (icd) allow = ::icd_hit_allowed(*icd, payload->element, payload->strength);
                    if (allow) {
                        es->pending_element  = payload->element;
                        es->pending_strength = payload->strength;
                    }
                }
                // 命中特效：元素色闪光 + 扩散光环 + 粒子迸溅
                if (fx_queue && payload) {
                    HitFxQueue::HitFx fx;
                    fx.x = bt->x;
                    fx.y = bt->y;
                    ElementColor ec = get_element_color(payload->element);
                    fx.r = ec.r; fx.g = ec.g; fx.b = ec.b;
                    fx.hit_type = 0;
                    // 尺寸系数：按敌人渲染半径自适应（20px 为基准 1.0）
                    auto* eri = world.get_component<RenderInfo>(en);
                    fx.scale = eri ? (eri->radius / 20.0f) : 1.0f;
                    fx_queue->items.push_back(fx);
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
                    // 撞墙特效：元素色火花（hit_type=1，墙体无半径自适应）
                    if (fx_queue && payload) {
                        HitFxQueue::HitFx fx;
                        fx.x = bt->x;
                        fx.y = bt->y;
                        ElementColor ec = get_element_color(payload->element);
                        fx.r = ec.r; fx.g = ec.g; fx.b = ec.b;
                        fx.hit_type = 1;
                        fx.scale = 1.0f;
                        fx_queue->items.push_back(fx);
                    }
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
