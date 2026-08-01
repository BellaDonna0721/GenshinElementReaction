#pragma once
#include "../core/World.h"
#include "../components/ElementStatus.h"
#include <cstdint>

// 敌人生成预设：把"不同种类敌人"的差异全部抽成配置数据（ECS 组合优于继承）
// 每个预设实例 = 一种敌人原型（火丘丘 / 水史莱姆 / 冰大怪 ……）
struct EnemyPreset {
    // 元素附着状态
    ElementType     element;   // 初始附着的元素类型（Hydro / Pyro / Cryo / Electro / None）
    ElementStrength strength;  // 元素强度（Weak / Medium / Strong / ExtraStrong）

    // 外形
    float       radius;        // 半径（像素）：默认碰撞和视觉一致

    // 颜色
    uint8_t     color_r;       // 红通道 0~255
    uint8_t     color_g;       // 绿通道 0~255
    uint8_t     color_b;       // 蓝通道 0~255
};

// ========== 内置常用敌人预设（元素量/时长 100% 按原神模型统一查表，别手填 decay_rate！） ==========

// 火丘丘：Pyro 弱元素（0.8 实际，9.5s），白色圆肉身（外层元素光环亮橙红），标准体型 20px
inline const EnemyPreset ENEMY_PYRO = {
    ElementType::Pyro,
    ElementStrength::Weak,
    20.0f,
    255, 255, 255
};

// 水史莱姆：Hydro 弱元素（0.8 实际，9.5s），白色圆，标准体型 20px
inline const EnemyPreset ENEMY_HYDRO = {
    ElementType::Hydro,
    ElementStrength::Weak,
    20.0f,
    255, 255, 255
};

// 冰大怪：Cryo 中元素（1.2 实际，10.75s），白色圆，体型更大 22px，元素更耐打
inline const EnemyPreset ENEMY_CRYO = {
    ElementType::Cryo,
    ElementStrength::Medium,
    22.0f,
    255, 255, 255
};

// 生成玩家实体
Entity spawn_player(World& world, float x, float y);

// 生成敌人实体（根据预设配置类型）
Entity spawn_enemy(World& world, float x, float y, const EnemyPreset& preset);
