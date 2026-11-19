#pragma once
#include <cstdint>
#include <vector>
#include <utility>

// =====================================================================
//  敌人 AI 组件集（组件组合区分行为类型，而非继承）
//
//  EnemyAI        —— 公共 AI 数据（所有敌人共享：速度/攻击/得分等）
//  StateMachineAI —— 状态机行为数据（巡逻/追击/逃跑，Boss 用）
//  RandomPatrolAI —— 随机巡逻行为数据（无目的漫游，普通小怪用）
//
//  设计原则：组件组合优于继承。新增 AI 类型 = 新组件 + EnemyAISystem 新 update 函数，
//  现有代码零改动（开闭原则）。
// =====================================================================

// AI 行为类型（用于 EnemyPreset 配置选择挂载哪个行为组件）
enum class AIType : uint8_t {
    StateMachine = 0,  // 三态状态机（巡逻/追击/逃跑）
    RandomPatrol = 1,  // 随机巡逻（不追击玩家）
};

// =====================================================================
//  EnemyAI —— 公共 AI 数据组件
//  所有敌人共享，与具体行为无关。
//  此处为默认值，会被 SpawnPreset 覆盖。
// =====================================================================
struct EnemyAI {    
    uint8_t enemy_type      = 0;       // 敌人类型（存 ElementType 值，血条显示用）
    float   move_speed      = 60.0f;   // 移动速度（像素/秒）
    float   alert_radius    = 180.0f;  // 警戒/探测范围（像素）
    float   attack_radius   = 36.0f;   // 攻击范围（像素，接触伤害）
    float   attack_power    = 10.0f;   // 每次接触伤害
    float   attack_cooldown = 0.6f;    // 攻击间隔（秒）
    float   attack_timer    = 0.0f;    // 当前攻击冷却计时
    int     score_value     = 10;      // 击杀得分
    float   home_x          = 0.0f;    // 出生点 X
    float   home_y          = 0.0f;    // 出生点 Y
};

// =====================================================================
//  StateMachineAI —— 状态机行为数据（Boss 用）
//  状态：Patrol / Chase / Flee
//  Chase 态依赖 PathfinderSystem::find_path 计算绕墙路线
// =====================================================================
enum class AIState : uint8_t {
    Patrol = 0,  // 巡逻：在出生点附近随机游走
    Chase  = 1,  // 追击：A* 路径追玩家，遇墙自动绕路
    Flee   = 2,  // 逃跑：血量低于阈值，朝玩家反方向逃离
};

struct StateMachineAI {
    AIState state           = AIState::Patrol;  // 当前状态
    float   flee_hp_ratio   = 0.25f;             // 血量比例低于此值进入逃跑
    float   patrol_timer    = 0.0f;              // 巡逻换点计时
    float   patrol_interval = 3.0f;              // 巡逻换点间隔（秒）
    float   patrol_radius   = 100.0f;            // 巡逻半径
    float   target_x        = 0.0f;              // 当前巡逻目标点 X
    float   target_y        = 0.0f;              // 当前巡逻目标点 Y

    // === A* 路径缓存（Chase 态消费） ===
    std::vector<std::pair<short, short>> path;     // cell 坐标路径（不含起点）
    int   path_index      = 0;                      // 当前走到第几个 waypoint
    float repath_timer    = 0.0f;                   // 重算冷却计时（最小间隔见 EnemyAISystem）
    short last_target_cx  = -1;                     // 上次目标（玩家）所在 cell x
    short last_target_cy  = -1;

    // === 撞墙避让（与 RandomPatrolAI 同口径） ===
    bool  blocked      = false;  // 上一帧被障碍阻挡（CollisionSystem 写入）
    float block_push_x = 0.0f;   // 推开方向（障碍→自身，未归一化，多障碍累加）
    float block_push_y = 0.0f;

    // === 路径跟随（Chase 态消费：平滑转向 + 卡死检测）===
    float heading_x   = 1.0f;   // 平滑后的移动方向（单位向量，独立于 Velocity 避免碰撞投影污染）
    float heading_y   = 0.0f;
    float stuck_timer = 0.0f;   // 持续被挡的累计时长（窗口位移检测用）
    float stuck_origin_x = 0.0f;// 进入被挡时的位置，用于判定窗口内是否真的没动
    float stuck_origin_y = 0.0f;

    // === 脱困态（Chase 态：卡死时锁定一个方向持续位移，优先于正常寻路）===
    float escape_x    = 0.0f;   // 锁定的脱困方向（单位向量）
    float escape_y    = 0.0f;
    float escape_timer = 0.0f;  // 脱困剩余时长，>0 表示处于脱困态
};

// =====================================================================
//  RandomPatrolAI —— 随机巡逻行为数据
//  敌人在出生点附近随机漫游，不主动追击玩家。
//  接触伤害仍由 CollisionSystem 处理（玩家主动撞上时生效）。
// =====================================================================
struct RandomPatrolAI {
    float   wander_radius   = 100.0f;  // 漫游范围（相对出生点）
    float   change_timer    = 0.0f;    // 换方向计时
    float   change_interval = 2.5f;    // 换方向间隔（秒）
    float   target_x        = 0.0f;    // 当前漫游目标点 X
    float   target_y        = 0.0f;    // 当前漫游目标点 Y

    // 被障碍阻挡标记：撞墙或被其他敌人挤开时由 CollisionSystem 写入，
    // 本帧 EnemyAISystem 消费后清零（AI 沿推开方向反向避让）。
    bool  blocked      = false;  // 上一帧被障碍阻挡
    float block_push_x = 0.0f;   // 推开方向（障碍→自身，未归一化，多障碍累加）
    float block_push_y = 0.0f;
};
