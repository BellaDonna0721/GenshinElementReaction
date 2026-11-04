#pragma once
#include <cstdint>
#include <vector>

// =====================================================================
//  GameState —— 全局游戏状态（单例组件，挂 Entity 0）
//
//  职责：
//    · 游戏流程阶段（Playing / LevelComplete / GameOver）
//    · 当前关卡编号、累计分数
//    · 已选增益列表（每关通关后追加，死亡后清零）
//    · 本关候选增益（LevelComplete 时由 LevelSystem 填入，等待 TS 选择）
//
//  不属于任何具体实体，因此挂在保留的单例实体 Entity 0 上。
// =====================================================================

enum class GamePhase : uint8_t {
    Playing       = 0,  // 游戏中
    LevelComplete = 1,  // 关卡通关，等待选增益
    GameOver      = 2,  // 玩家死亡
};

// 增益类型（与 BuffRegistry 中的索引一一对应）
enum class BuffType : uint8_t {
    AttackUp        = 0,  // 攻击力 +20%
    AttackSpeedUp   = 1,  // 攻速 +15%（冷却 -15%）
    MoveSpeedUp     = 2,  // 移速 +15%
    MaxHpUp         = 3,  // 血量上限 +30 并回满
    Heal            = 4,  // 立即回复 50% 血量
    ElementDamageUp = 5,  // 元素反应伤害 +25%
    BulletDamageUp  = 6,  // 子弹直射伤害 +5
    Shield          = 7,  // 获得一层护盾
    MasteryUp       = 8,  // 元素精通 +5
    Count           = 9,  // 增益总数
};

struct GameState {
    GamePhase phase          = GamePhase::Playing;
    int       level          = 1;          // 当前关卡编号（从 1 开始）
    int       score          = 0;          // 累计分数
    bool      level_cleared  = false;      // 本关是否已通关（用于触发一次 LevelComplete）
    bool      is_training    = false;      // 是否为训练模式（训练模式下 LevelSystem 不运行）

    std::vector<BuffType> active_buffs;    // 已选增益（死亡后清零）
    std::vector<BuffType> buff_choices;    // 本关候选增益（3 个，LevelComplete 时填入）

    // 护盾层数（由 Shield 增益提供，受伤优先消耗护盾）
    int       shield_count   = 0;

    // 元素精通：剧变反应（超载/感电/超导/扩散）的伤害系数
    //   剧变伤害 = 精通 × 反应系数（超载2.0/感电1.2/超导1.0/扩散1.2）
    //   初始值 2.0 → 超导伤害 = 2×1.0 = 2 = 普攻10的1/5
    float     elemental_mastery = 2.0f;
};
