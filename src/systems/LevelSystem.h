#pragma once
#include "../systems/System.h"
#include "../components/GameState.h"
#include "../gameplay/LevelConfig.h"
#include <cstdint>

// =====================================================================
//  LevelSystem —— 关卡管理系统
//
//  职责：
//    · 加载关卡（生成墙体、定位玩家、启动波次）
//    · 按波次配置定时刷怪
//    · 通关判定（所有波次完成 + 场上无敌人）
//    · Boss 关通关后随机生成 3 个增益候选，写入 GameState
//
//  流水线位置：EnemyAISystem 之后、CollisionSystem 之前
//  （先刷怪，AI 再处理新敌人）
// =====================================================================

class LevelSystem : public System {
public:
    void update(World& world, float dt) override;

    // 加载指定关卡（1-based）
    void load_level(World& world, int level_number);

    // 重置到第 1 关
    void reset(World& world);

    // 应用增益并进入下一关
    void apply_buff_and_next(World& world, BuffType buff);

    // 当前是否处于 LevelComplete 等待选增益
    bool is_level_complete() const { return m_level_complete; }

private:
    void start_wave(World& world);
    void complete_level(World& world);
    void pick_buff_choices(World& world);

    int    m_level_index      = 0;    // 当前关卡索引（0-based）
    int    m_wave_index       = 0;    // 当前波次索引
    float  m_wave_timer       = 0.0f; // 波次开始倒计时
    bool   m_wave_started     = false; // 当前波次是否已开始
    int    m_spawn_cursor     = 0;    // 当前波次已处理到第几个 spawn
    int    m_spawned_count    = 0;    // 当前 spawn 已生成数量
    float  m_spawn_timer      = 0.0f; // 当前 spawn 生成间隔计时
    bool   m_level_complete   = false; // 本关是否已通关
    bool   m_all_waves_done   = false; // 所有波次是否已生成完毕
};
