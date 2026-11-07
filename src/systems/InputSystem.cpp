#include "InputSystem.h"
#include "../components/Transform.h"
#include "../components/Velocity.h"
#include "../components/Lifetime.h"
#include "../components/ElementPayload.h"
#include "../components/SphereCollider.h"
#include "../components/IdentityTag.h"
#include "../components/ElementStatus.h"
#include "../components/RenderInfo.h"
#include "../components/FacingDirection.h"
#include "../components/GameState.h"
#include "../gameplay/BuffRegistry.h"
#include "../utils/element_color.h"
#include <cmath>
#include <algorithm>

InputState g_input;

// ========== InputSystem ==========

void InputSystem::update(World& world, float dt) {
    if (m_fire_cooldown > 0.0f) m_fire_cooldown -= dt;

    // 读取全局增益
    GameState* gs = world.get_component<GameState>(0);
    float atk_speed_mult = 1.0f;
    if (gs) {
        for (auto b : gs->active_buffs) {
            if (b == BuffType::AttackSpeedUp) atk_speed_mult -= BuffRegistry::get(b).value;
        }
    }
    float fire_interval = FIRE_INTERVAL * atk_speed_mult;

    // 用 IdentityTag 过滤玩家：逻辑层不读 RenderInfo.entity_type
    for (auto e : world.query<Transform, Velocity, FacingDirection, IdentityTag, RenderInfo>()) {
        auto* id = world.get_component<IdentityTag>(e);
        if (id->identity_type != Identity::Player) continue;

        auto* t    = world.get_component<Transform>(e);
        auto* v    = world.get_component<Velocity>(e);
        auto* face = world.get_component<FacingDirection>(e);
        auto* ri   = world.get_component<RenderInfo>(e);

        // ========== ① 数字键切换玩家元素（1~7）==========
        if (g_input.element_key >= 1 && g_input.element_key <= 7) {
            ElementType new_elem = s_key_to_elem[g_input.element_key];
            if (new_elem != ElementType::None) {
                world.set_player_current_element(new_elem);
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

        // --- 射击 ---
        bool want_fire = g_input.mouse_click || g_input.mouse_right_click;
        if (want_fire && m_fire_cooldown <= 0.0f) {
            m_fire_cooldown = fire_interval;
            ElementType elem = world.get_player_current_element();
            if (elem == ElementType::None) elem = ElementType::Pyro;
            ElementStrength strength = g_input.mouse_right_click
                                         ? ElementStrength::Strong
                                         : ElementStrength::Weak;
            fire(world, t->x, t->y, face->dx, face->dy, elem, strength, gs);
        }
    }
}

void InputSystem::fire(World& world, float from_x, float from_y, float dir_x, float dir_y,
                       ElementType elem, ElementStrength strength, GameState* gs) {
    Entity e = g_bullet_pool.acquire();
    if (e == INVALID_ENTITY) return;

    // 子弹颜色统一用元素颜色
    ElementColor bc = get_element_color(elem);

    // ECS 化装配
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

    // 攻击力：基础 10，叠加攻击增益
    //   AttackUp: +20%（百分比）
    //   BulletDamageUp: +5（固定值）
    float base_damage = 10.0f;
    if (gs) {
        for (auto b : gs->active_buffs) {
            if (b == BuffType::AttackUp)        base_damage *= (1.0f + BuffRegistry::get(b).value);
            else if (b == BuffType::BulletDamageUp) base_damage += BuffRegistry::get(b).value;
        }
    }

    ElementPayload payload;
    payload.element     = elem;
    payload.strength    = strength;
    payload.base_damage = base_damage;
    world.add_component<ElementPayload>(e, payload);

    SphereCollider col;
    col.radius = 6.0f;
    world.add_component<SphereCollider>(e, col);

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
