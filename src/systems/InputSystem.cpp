#include "InputSystem.h"
#include "../components/Transform.h"
#include "../components/Velocity.h"
#include "../components/Lifetime.h"
#include "../components/ElementPayload.h"
#include "../components/Collider.h"
#include "../components/IdentityTag.h"
#include "../components/ElementStatus.h"
#include "../components/RenderInfo.h"
#include "../components/FacingDirection.h"
#include "../utils/element_color.h"   //get_element_color()：7种元素RGB统一映射
#include <cmath>

InputState g_input;
BulletPool g_bullet_pool;

// ========== BulletPool ==========

void BulletPool::preallocate(World& world) {
    m_entities.clear();
    m_in_use.clear();
    for (int i = 0; i < POOL_SIZE; ++i) {
        m_entities.push_back(world.create_entity());
        m_in_use.push_back(false);
    }
}

Entity BulletPool::acquire() {
    for (int i = 0; i < (int)m_entities.size(); ++i) {
        if (!m_in_use[i]) {
            m_in_use[i] = true;
            return m_entities[i];
        }
    }
    return INVALID_ENTITY;
}

void BulletPool::release(Entity e) {
    for (int i = 0; i < (int)m_entities.size(); ++i) {
        if (m_entities[i] == e && m_in_use[i]) {
            m_in_use[i] = false;
            return;
        }
    }
}

bool BulletPool::is_pooled(Entity e) const {
    for (auto id : m_entities) if (id == e) return true;
    return false;
}

// ========== 共享工具：归还子弹实体到对象池 ==========

void release_bullet_entity(World& world, Entity e) {
    if (!g_bullet_pool.is_pooled(e)) {
        world.destroy_entity(e);
        return;
    }
    // 移除子弹发射时 add 的 7 个组件（Transform/Velocity/Lifetime/ElementPayload/Collider/IdentityTag/RenderInfo）
    world.remove_component<Transform>(e);
    world.remove_component<Velocity>(e);
    world.remove_component<Lifetime>(e);
    world.remove_component<ElementPayload>(e);
    world.remove_component<Collider>(e);
    world.remove_component<IdentityTag>(e);
    world.remove_component<RenderInfo>(e);
    g_bullet_pool.release(e);
}

// ========== InputSystem ==========

void InputSystem::update(World& world, float dt) {
    if (m_fire_cooldown > 0.0f) m_fire_cooldown -= dt;

    // 用 IdentityTag 过滤玩家：逻辑层不读 RenderInfo.entity_type
    for (auto e : world.query<Transform, Velocity, FacingDirection, ElementStatus, IdentityTag, RenderInfo>()) {
        auto* id = world.get_component<IdentityTag>(e);
        if (id->identity_type != Identity::Player) continue;

        auto* t    = world.get_component<Transform>(e);
        auto* v    = world.get_component<Velocity>(e);
        auto* face = world.get_component<FacingDirection>(e);
        auto* pe   = world.get_component<ElementStatus>(e);
        auto* ri   = world.get_component<RenderInfo>(e);

        // ========== ① 数字键切换玩家元素（1~7）==========
        // TS 侧按键按下的那一帧脉冲写入 element_key（每帧归零），只切一次
        if (g_input.element_key >= 1 && g_input.element_key <= 7) {
            ElementType new_elem = s_key_to_elem[g_input.element_key];
            if (new_elem != ElementType::None) {
                // 复用玩家已有的 ElementStatus：把类型改成新元素
                // (gauge/duration/decay_rate 暂时不碰，切元素只改变"身份+颜色"，不强制改存量
                pe->type = new_elem;

                // 同步改 RenderInfo 颜色（视觉跟着变）
                ElementColor c = get_element_color(new_elem);
                ri->color_r = c.r;
                ri->color_g = c.g;
                ri->color_b = c.b;
            }
        }

        // --- 移动：WASD ---
        v->vx = 0.0f;
        v->vy = 0.0f;
        if (g_input.move_up)    v->vy = -1.0f;
        if (g_input.move_down)  v->vy =  1.0f;
        if (g_input.move_left)  v->vx = -1.0f;
        if (g_input.move_right) v->vx =  1.0f;
        float len = sqrtf(v->vx * v->vx + v->vy * v->vy);
        if (len > 1.0f) { v->vx /= len; v->vy /= len; }

        // --- 朝向：跟随鼠标 ---
        float dx = g_input.mouse_x - t->x;
        float dy = g_input.mouse_y - t->y;
        float dlen = sqrtf(dx * dx + dy * dy);
        if (dlen > 0.5f) {
            face->dx = dx / dlen;
            face->dy = dy / dlen;
        }

        // --- 射击：鼠标点击，有冷却，沿朝向发射；子弹元素=玩家当前元素 ---
        if (g_input.mouse_click && m_fire_cooldown <= 0.0f) {
            m_fire_cooldown = FIRE_INTERVAL;
            ElementType elem = (pe && pe->type != ElementType::None) ? pe->type : ElementType::Pyro;
            fire(world, t->x, t->y, face->dx, face->dy, elem);
        }
    }
}

void InputSystem::fire(World& world, float from_x, float from_y, float dir_x, float dir_y, ElementType elem) {
    Entity e = g_bullet_pool.acquire();
    if (e == INVALID_ENTITY) return;

    // 子弹颜色统一用元素颜色（和玩家切换后的颜色一致）
    ElementColor bc = get_element_color(elem);

    // ECS 化装配：7 个单一职责组件（身份统一用 IdentityTag::Bullet）
    Transform tform;
    tform.x = from_x + dir_x * 28.0f;
    tform.y = from_y + dir_y * 28.0f;
    world.add_component<Transform>(e, tform);

    Velocity vel;
    vel.vx = dir_x;
    vel.vy = dir_y;
    world.add_component<Velocity>(e, vel);

    Lifetime life;
    life.remaining = 2.5f;
    world.add_component<Lifetime>(e, life);

    // 关键：子弹元素=玩家当前元素，强度固定 Weak（对应 0.8 实际 + 9.5s）
    ElementPayload payload;
    payload.element  = elem;
    payload.strength = ElementStrength::Weak;
    world.add_component<ElementPayload>(e, payload);

    Collider col;
    col.radius = 6.0f;
    world.add_component<Collider>(e, col);

    IdentityTag bid;
    bid.identity_type = Identity::Bullet;
    world.add_component<IdentityTag>(e, bid);

    RenderInfo ri;
    ri.entity_type = Identity::Bullet;
    ri.radius      = 6.0f;
    ri.color_r     = bc.r;   // 子弹颜色=元素颜色（火=红/水=深蓝/雷=紫…）
    ri.color_g     = bc.g;
    ri.color_b     = bc.b;
    ri.color_a     = 255;
    world.add_component<RenderInfo>(e, ri);
}
