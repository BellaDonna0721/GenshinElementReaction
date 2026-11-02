#include "MovementSystem.h"
#include "../components/Transform.h"
#include "../components/Velocity.h"
#include "../components/IdentityTag.h"
#include "../components/EnemyAI.h"
#include "../components/GameState.h"
#include "../gameplay/BuffRegistry.h"
#include "../gameplay/BulletPool.h"

void MovementSystem::update(World& world, float dt) {
    const float PLAYER_SPEED   = 200.0f;
    const float BULLET_SPEED   = 520.0f;
    const float WORLD_W        = 1000.0f;
    const float WORLD_H        = 700.0f;

    // 玩家移速增益
    float player_speed_mult = 1.0f;
    GameState* gs = world.get_component<GameState>(0);
    if (gs) {
        for (auto b : gs->active_buffs) {
            if (b == BuffType::MoveSpeedUp) player_speed_mult += BuffRegistry::get(b).value;
        }
    }

    std::vector<Entity> out_of_bounds;

    for (auto e : world.query<Transform, Velocity>()) {
        auto* t = world.get_component<Transform>(e);
        auto* v = world.get_component<Velocity>(e);

        auto* id_tag = world.get_component<IdentityTag>(e);
        bool is_bullet = id_tag && id_tag->identity_type == Identity::Bullet;
        bool is_player = id_tag && id_tag->identity_type == Identity::Player;

        float speed;
        if (is_bullet) {
            speed = BULLET_SPEED;
        } else if (is_player) {
            speed = PLAYER_SPEED * player_speed_mult;
        } else {
            // 敌人：使用 EnemyAI 的 move_speed
            auto* ai = world.get_component<EnemyAI>(e);
            speed = ai ? ai->move_speed : PLAYER_SPEED;
        }

        t->x += v->vx * speed * dt;
        t->y += v->vy * speed * dt;

        if (is_bullet) {
            if (t->x < -20.0f || t->x > WORLD_W + 20.0f ||
                t->y < -20.0f || t->y > WORLD_H + 20.0f)
            {
                out_of_bounds.push_back(e);
            }
        } else {
            if (t->x < 20.0f)           t->x = 20.0f;
            if (t->x > WORLD_W - 20.0f) t->x = WORLD_W - 20.0f;
            if (t->y < 20.0f)           t->y = 20.0f;
            if (t->y > WORLD_H - 20.0f) t->y = WORLD_H - 20.0f;
        }
    }

    // 处理出界子弹
    for (auto e : out_of_bounds) {
        release_bullet_entity(world, e);
    }
}
