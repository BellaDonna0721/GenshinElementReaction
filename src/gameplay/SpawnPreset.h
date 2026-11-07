#pragma once
#include "../core/World.h"
#include "../utils/ElementType.h"
#include "../components/EnemyAI.h"
#include <cstdint>

// 敌人生成预设：把"不同种类敌人"的差异全部抽成配置数据（ECS 组合优于继承）
// 每个预设实例 = 一种敌人原型（火丘丘 / 水史莱姆 / 冰大怪 ……）
struct EnemyPreset {
    // 元素附着状态
    ElementType     element;   // 初始附着的元素类型（Hydro / Pyro / Cryo / Electro / None）
    ElementStrength strength;  // 元素强度（Weak / Medium / Strong / ExtraStrong）

    // 战斗数值
    float           max_hp;    // 最大血量（Health 组件初始值）

    // 外形
    float       radius;        // 半径（像素）：默认碰撞和视觉一致

    // 颜色
    uint8_t     color_r;       // 红通道 0~255
    uint8_t     color_g;       // 绿通道 0~255
    uint8_t     color_b;       // 蓝通道 0~255

    // 冻结免疫（true=可冻结，false=冻元素立即消失）
    bool        can_be_frozen = true;

    // ===== AI 参数（EnemyAI 组件初始值）=====
    AIType      ai_type        = AIType::RandomPatrol;  // AI 行为类型（小怪随机巡逻，Boss 用状态机）
    float       move_speed     = 60.0f;   // 移动速度（像素/秒）
    float       alert_radius   = 180.0f;  // 警戒范围
    float       attack_radius  = 36.0f;   // 攻击范围（接触伤害）
    float       attack_power   = 10.0f;   // 每次接触伤害
    int         score_value    = 10;      // 击杀得分
};

// ========== 内置常用敌人预设（元素量/时长 100% 按原神模型统一查表，别手填 decay_rate！） ==========

// 火丘丘：Pyro 弱元素（0.8 实际，9.5s），白色圆肉身（外层元素光环亮橙红），标准体型 20px
inline const EnemyPreset ENEMY_PYRO = {
    ElementType::Pyro,          // element：初始附着元素
    ElementStrength::Weak,      // strength：元素强度档
    50.0f,                      // max_hp：最大血量
    20.0f,                      // radius：体型半径（像素）
    255, 255, 255,              // color_r/g/b：颜色
    true,                       // can_be_frozen：是否可被冻结
    AIType::RandomPatrol,       // ai_type：AI 行为类型
    60.0f,                      // move_speed：移动速度（像素/秒）
    180.0f,                     // alert_radius：警戒范围（像素）
    36.0f,                      // attack_radius：攻击范围（像素，接触伤害）
    10.0f,                      // attack_power：每次接触伤害
    10,                         // score_value：击杀得分
};

// 水史莱姆：Hydro 弱元素（0.8 实际，9.5s），白色圆，标准体型 20px
inline const EnemyPreset ENEMY_HYDRO = {
    ElementType::Hydro,         // element：初始附着元素
    ElementStrength::Weak,      // strength：元素强度档
    50.0f,                      // max_hp：最大血量
    20.0f,                      // radius：体型半径（像素）
    255, 255, 255,              // color_r/g/b：颜色
    true,                       // can_be_frozen：是否可被冻结
    AIType::RandomPatrol,       // ai_type：AI 行为类型
    60.0f,                      // move_speed：移动速度（像素/秒）
    180.0f,                     // alert_radius：警戒范围（像素）
    36.0f,                      // attack_radius：攻击范围（像素，接触伤害）
    10.0f,                      // attack_power：每次接触伤害
    10,                         // score_value：击杀得分
};

// 冰大怪：Cryo 中元素（1.2 实际，10.75s），白色圆，体型更大 22px，元素更耐打
inline const EnemyPreset ENEMY_CRYO = {
    ElementType::Cryo,          // element：初始附着元素
    ElementStrength::Medium,    // strength：元素强度档
    80.0f,                      // max_hp：最大血量
    22.0f,                      // radius：体型半径（像素）
    255, 255, 255,              // color_r/g/b：颜色
    true,                       // can_be_frozen：是否可被冻结
    AIType::RandomPatrol,       // ai_type：AI 行为类型
    60.0f,                      // move_speed：移动速度（像素/秒）
    180.0f,                     // alert_radius：警戒范围（像素）
    36.0f,                      // attack_radius：攻击范围（像素，接触伤害）
    10.0f,                      // attack_power：每次接触伤害
    10,                         // score_value：击杀得分
};

// 测试木桩：Hydro 弱元素，血量 99999（用于长时间测试元素反应/冻结）
inline const EnemyPreset ENEMY_TEST = {
    ElementType::Hydro,         // element：初始附着元素
    ElementStrength::Weak,      // strength：元素强度档
    99999.0f,                   // max_hp：最大血量（极高，测试用）
    20.0f,                      // radius：体型半径（像素）
    255, 255, 255,              // color_r/g/b：颜色
    true,                       // can_be_frozen：是否可被冻结
    AIType::RandomPatrol,       // ai_type：AI 行为类型
    0.0f,                       // move_speed：移动速度（像素/秒，训练木桩不移动）
    180.0f,                     // alert_radius：警戒范围（像素）
    36.0f,                      // attack_radius：攻击范围（像素，接触伤害）
    0.0f,                      // attack_power：每次接触伤害
    10,                         // score_value：击杀得分
};

// Boss：高血量、大体型、高攻击力、高分数
//   元素为 Electro（雷），中元素强度，体型 36px，血量 300
//   移速稍慢但攻击范围大，适合作为关卡守关 BOSS
inline const EnemyPreset ENEMY_BOSS = {
    ElementType::Electro,       // element：初始附着元素
    ElementStrength::Medium,    // strength：元素强度档
    300.0f,                     // max_hp：最大血量
    36.0f,                      // radius：体型半径（像素，大体型）
    180, 120, 220,              // color_r/g/b：颜色（紫色）
    true,                       // can_be_frozen：是否可被冻结
    AIType::StateMachine,       // ai_type：AI 行为类型（Boss 用状态机）
    50.0f,                      // move_speed：移动速度（像素/秒，稍慢）
    220.0f,                     // alert_radius：警戒范围（像素）
    50.0f,                      // attack_radius：攻击范围（像素，接触伤害）
    20.0f,                      // attack_power：每次接触伤害（高伤害）
    100,                        // score_value：击杀得分（高分）
};

// 生成玩家实体
Entity spawn_player(World& world, float x, float y);

// 生成敌人实体（根据预设配置类型）
Entity spawn_enemy(World& world, float x, float y, const EnemyPreset& preset);

// 生成墙体实体（静态矩形障碍物）
//   x, y = 矩形中心坐标
//   w, h = 矩形宽高
//   颜色默认石墙灰，可通过 RenderInfo 自定义
Entity spawn_wall(World& world, float x, float y, float w, float h);
