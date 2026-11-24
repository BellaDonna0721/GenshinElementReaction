#pragma once
#include <cstdint>
#include <vector>
#include "../../utils/ElementType.h"

// =====================================================================
//  GameState —— 全局游戏状态（单例组件，挂 Entity 0）
//
//  职责：
//    · 游戏流程阶段（Playing / LevelComplete / GameOver）
//    · 当前关卡编号、累计分数
//    · 波次进度（wave_index / wave_timer / wave_spawned / all_waves_done）—— 关卡状态唯一真源
//    · 玩家战斗状态（当前射击元素 player_current_element / 元素精通 / 护盾）
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
    Shield          = 7,  // 护盾层数上限 +1
    MasteryUp       = 8,  // 元素精通 +5
    Count           = 9,  // 增益总数
};

struct GameState {
    GamePhase phase          = GamePhase::Playing;
    int       level          = 1;          // 当前关卡编号（从 1 开始）
    int       score          = 0;          // 累计分数
    bool      level_cleared  = false;      // 本关是否已通关
    bool      is_training    = false;      // 是否为训练模式（训练模式下 LevelSystem 不运行）

    // —— 波次进度（由 LevelSystem 读写，关卡状态唯一真源，系统本身无状态）——
    int   wave_index      = 0;             // 当前波次索引（0-based）
    float wave_timer      = 0.0f;          // 下一波开始倒计时（秒）
    bool  wave_spawned    = false;         // 本波已刷完，等待敌人全部消灭
    bool  all_waves_done  = false;         // 所有波次已刷完且敌人全灭

    std::vector<BuffType> active_buffs;    // 已选增益（死亡后清零）
    std::vector<BuffType> buff_choices;    // 本关候选增益（3 个，LevelComplete 时填入）

    // 护盾层数（由结晶晶片拾取获得，受伤优先消耗护盾）
    int       shield_count   = 0;
    // 护盾层数上限（默认 5，可选 Shield 增益 +1）
    int       shield_max     = 5;

    // 玩家当前选中的射击元素（初始默认 Pyro，数字键 1~7 切换）
    ElementType player_current_element = ElementType::Pyro;

    // 元素精通：剧变反应（超载/感电/超导/扩散）的伤害系数
    //   剧变伤害 = 精通 × 反应系数（超载2.0/感电1.2/超导1.0/扩散1.2）
    //   初始值 2.0 → 超导伤害 = 2×1.0 = 2 = 普攻10的1/5
    float     elemental_mastery = 2.0f;
};
