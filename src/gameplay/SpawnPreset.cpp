#include "SpawnPreset.h"
#include "../components/Transform.h"
#include "../components/Velocity.h"
#include "../components/ElementStatus.h"
#include "../components/RenderInfo.h"
#include "../components/FacingDirection.h"
#include "../components/Collider.h"
#include "../components/IdentityTag.h"

// ========== 玩家工厂（标准配置，目前没有多种玩家变体） ==========

Entity spawn_player(World& world, float x, float y) {
    Entity e = world.create_entity();

    // 1. 位置
    Transform tform;
    tform.x = x;
    tform.y = y;
    world.add_component<Transform>(e, tform);

    // 2. 速度（初始静止，WASD 驱动）
    Velocity vel;
    vel.vx = 0.0f;
    vel.vy = 0.0f;
    world.add_component<Velocity>(e, vel);

    // 3. 元素状态（玩家初始身上无附着元素；射击元素默认 Pyro，可通过数字键 1~8 切换）
    ElementStatus elem;
    elem.current_player_elem = ElementType::Pyro;
    world.add_component<ElementStatus>(e, elem);

    // 4. 朝向（初始朝右，InputSystem 每帧跟随鼠标覆盖）
    FacingDirection face;
    face.dx = 1.0f;
    face.dy = 0.0f;
    world.add_component<FacingDirection>(e, face);

    // 5. 渲染信息（火红 22px 圆，和 Pyro 元素色一致）
    RenderInfo ri;
    ri.entity_type = Identity::Player;
    ri.radius      = 22.0f;
    ri.color_r     = 255;
    ri.color_g     = 70;
    ri.color_b     = 50;
    ri.color_a     = 255;
    world.add_component<RenderInfo>(e, ri);

    // 6. 碰撞半径
    Collider col;
    col.radius = 22.0f;
    world.add_component<Collider>(e, col);

    // 7. 逻辑身份
    IdentityTag id;
    id.identity_type = Identity::Player;
    world.add_component<IdentityTag>(e, id);

    return e;
}

// ========== 敌人工厂（通用版：按 EnemyPreset 配置组装组件） ==========

Entity spawn_enemy(World& world, float x, float y, const EnemyPreset& preset) {
    Entity e = world.create_entity();

    // 1. 位置
    Transform tform;
    tform.x = x;
    tform.y = y;
    world.add_component<Transform>(e, tform);

    // 2. 速度
    Velocity vel;
    vel.vx = 0.0f;
    vel.vy = 0.0f;
    world.add_component<Velocity>(e, vel);

    // 3. 元素状态（从 preset.element + preset.strength 统一查表：×0.8 八折、duration、decay_rate 全是原神模型）
    ElementStatus elem;
    attach_element(elem, preset.element, preset.strength);
    world.add_component<ElementStatus>(e, elem);

    // 4. 渲染信息（颜色+半径来自 preset）
    RenderInfo ri;
    ri.entity_type = Identity::Enemy;
    ri.radius      = preset.radius;
    ri.color_r     = preset.color_r;
    ri.color_g     = preset.color_g;
    ri.color_b     = preset.color_b;
    ri.color_a     = 255;
    world.add_component<RenderInfo>(e, ri);

    // 5. 碰撞半径 和视觉半径一致
    Collider col;
    col.radius = preset.radius;
    world.add_component<Collider>(e, col);

    // 6. 逻辑身份（Enemy）
    IdentityTag id;
    id.identity_type = Identity::Enemy;
    world.add_component<IdentityTag>(e, id);

    return e;
}
