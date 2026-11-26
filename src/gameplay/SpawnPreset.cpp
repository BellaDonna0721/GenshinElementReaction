#include "SpawnPreset.h"
#include "../components/Transform.h"
#include "../components/Velocity.h"
#include "../components/ElementStatus.h"
#include "../components/ElementICDStatus.h"
#include "../components/RenderInfo.h"
#include "../components/FacingDirection.h"
#include "../components/SphereCollider.h"
#include "../components/IdentityTag.h"
#include "../components/Health.h"
#include "../components/BoxCollider.h"
#include "../components/EnemyAI.h"
#include "../components/StateMachineAI.h"
#include "../components/PatrolAI.h"
#include "../components/PathFollowAI.h"
#include "../components/ObstacleContact.h"
#include "../components/Cooldown.h"
#include "../components/singletonComponents/GameState.h"

// ========== 玩家工厂（标准配置，目前没有多种玩家变体） ==========

Entity spawn_player(World& world, float x, float y) {
    Entity e = world.create_entity();

    // 1. 位置
    Transform tform;
    tform.x = x;
    tform.y = y;
    world.add_component<Transform>(e, tform);

    // 2. 速度（初始静止，WASD 驱动；speed 为移速标量，与方向配合）
    Velocity vel;
    vel.vx    = 0.0f;
    vel.vy    = 0.0f;
    vel.speed = 200.0f;
    world.add_component<Velocity>(e, vel);

    // 3. 元素状态（玩家初始身上无附着元素）
    ElementStatus elem;
    world.add_component<ElementStatus>(e, elem);

    // 3b. 元素附着 ICD（玩家自身也带，因为规则⑤要求 ICD 对所有 ElementStatus 实体生效）
    ElementICDStatus icd;
    world.add_component<ElementICDStatus>(e, icd);

    // 玩家初始射击元素默认 Pyro（存放在 GameState 单例组件）
    GameState* gs = world.get_component<GameState>(0);
    if (gs) gs->player_current_element = ElementType::Pyro;

    // 4. 朝向（初始朝右，InputSystem 每帧跟随鼠标覆盖）
    FacingDirection face;
    face.dx = 1.0f;
    face.dy = 0.0f;
    world.add_component<FacingDirection>(e, face);

    // 5. 渲染信息（火红 22px 圆，和 Pyro 元素色一致）
    RenderInfo ri;
    ri.radius      = 22.0f;
    ri.color_r     = 255;
    ri.color_g     = 70;
    ri.color_b     = 50;
    ri.color_a     = 255;
    world.add_component<RenderInfo>(e, ri);

    // 6. 碰撞半径
    SphereCollider col;
    col.radius = 22.0f;
    world.add_component<SphereCollider>(e, col);

    // 7. 逻辑身份
    IdentityTag id;
    id.identity_type = Identity::Player;
    world.add_component<IdentityTag>(e, id);

    // 8. 血量（预留：当前没有系统对玩家造成伤害，组件就位便于未来扩展）
    Health hp;
    hp.max_hp     = 100.0f;
    hp.current_hp = 100.0f;
    world.add_component<Health>(e, hp);

    // 9. 射击冷却（取代 InputSystem::m_fire_cooldown 违规成员变量）
    //    duration 基础值 0.5s；InputSystem 触发时按攻击速度 buff 动态乘以 atk_speed_mult
    Cooldown fire_cd;
    fire_cd.duration  = 0.5f;
    fire_cd.remaining = 0.0f;  // 初始就绪
    world.add_component<Cooldown>(e, fire_cd);

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

    // 2. 速度（speed 真源在 EnemyPreset.move_speed）
    Velocity vel;
    vel.vx    = 0.0f;
    vel.vy    = 0.0f;
    vel.speed = preset.move_speed;
    world.add_component<Velocity>(e, vel);

    // 3. 元素状态（从 preset.element + preset.strength 统一查表：×0.8 八折、duration、decay_rate 全是原神模型）
    ElementStatus elem;
    attach_element(elem, preset.element, preset.strength);
    elem.frozen.can_freeze = preset.can_be_frozen;  // 冻结免疫配置
    world.add_component<ElementStatus>(e, elem);

    // 3b. 元素附着 ICD（每个敌人独立一套 Weak/Strong 计数+计时）
    ElementICDStatus icd;
    world.add_component<ElementICDStatus>(e, icd);

    // 4. 渲染信息（颜色+半径来自 preset）
    RenderInfo ri;
    ri.radius      = preset.radius;
    ri.color_r     = preset.color_r;
    ri.color_g     = preset.color_g;
    ri.color_b     = preset.color_b;
    ri.color_a     = 255;
    world.add_component<RenderInfo>(e, ri);

    // 5. 碰撞半径 和视觉半径一致
    SphereCollider col;
    col.radius = preset.radius;
    world.add_component<SphereCollider>(e, col);

    // 6. 逻辑身份（Enemy）
    IdentityTag id;
    id.identity_type = Identity::Enemy;
    world.add_component<IdentityTag>(e, id);

    // 7. 血量（按预设配置）
    Health hp;
    hp.max_hp     = preset.max_hp;
    hp.current_hp = preset.max_hp;
    world.add_component<Health>(e, hp);

    // 8. AI 公共数据（参数来自预设，attack_cooldown 已抽成独立 Cooldown 组件）
    EnemyAI ai;
    ai.ai_type       = preset.ai_type;
    ai.enemy_type    = static_cast<uint8_t>(preset.element);  // 临时：存元素类型用于血条显示类型名
    ai.alert_radius   = preset.alert_radius;
    ai.attack_radius  = preset.attack_radius;
    ai.attack_power   = preset.attack_power;
    ai.score_value    = preset.score_value;
    ai.home_x         = x;
    ai.home_y         = y;
    world.add_component<EnemyAI>(e, ai);

    // 8b. 攻击冷却组件（取代 EnemyAI::attack_timer + attack_cooldown）
    Cooldown atk_cd;
    atk_cd.duration  = preset.attack_cooldown;  // 配置真源仍在 EnemyPreset
    atk_cd.remaining = 0.0f;  // 初始就绪，进 CollisionSystem 第一帧即可攻击
    world.add_component<Cooldown>(e, atk_cd);

    // 9. 挂载巡逻组件（通用：小怪随机巡逻 / Boss 状态机 Patrol 态共用）
    PatrolAI pc;
    pc.target_x        = x;
    pc.target_y        = y;
    pc.patrol_interval = preset.patrol_interval;
    pc.patrol_radius   = preset.patrol_radius;
    pc.leash_radius    = preset.leash_radius;
    world.add_component<PatrolAI>(e, pc);

    // 10. 状态机附加组件（仅 ai_type==StateMachine 时挂载，Boss 用）
    if (preset.ai_type == AIType::StateMachine) {
        StateMachineAI sm;
        sm.flee_hp_ratio = preset.flee_hp_ratio;
        world.add_component<StateMachineAI>(e, sm);

        PathFollowAI pf;  // A* 寻路追击执行器
        world.add_component<PathFollowAI>(e, pf);
    }

    // 11. 挂载障碍接触反馈组件（CollisionSystem 写，AI 系统消费，与行为类型无关）
    world.add_component<ObstacleContact>(e, ObstacleContact{});

    return e;
}

// ========== 墙体工厂（静态矩形障碍物）==========
//  墙体是纯静态实体：无 Velocity / Health / ElementStatus / Lifetime
//  只携带位置、矩形碰撞体、身份、渲染颜色四个组件

Entity spawn_wall(World& world, float x, float y, float w, float h) {
    Entity e = world.create_entity();

    // 1. 位置（矩形中心）
    Transform tform;
    tform.x = x;
    tform.y = y;
    world.add_component<Transform>(e, tform);

    // 2. 矩形碰撞体（宽高）
    BoxCollider rc;
    rc.width  = w;
    rc.height = h;
    world.add_component<BoxCollider>(e, rc);

    // 3. 逻辑身份（Wall）
    IdentityTag id;
    id.identity_type = Identity::Wall;
    world.add_component<IdentityTag>(e, id);

    // 4. 渲染颜色（石墙灰，供 OutputSystem 墙体通道读取）
    RenderInfo ri;
    ri.radius      = 0.0f;  // 墙体无视觉半径，此字段无意义
    ri.color_r     = 120;
    ri.color_g     = 120;
    ri.color_b     = 130;
    ri.color_a     = 255;
    world.add_component<RenderInfo>(e, ri);

    return e;
}
