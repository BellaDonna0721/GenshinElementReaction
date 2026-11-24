#pragma once
#include "../systems/System.h"
#include "../components/singletonComponents/GameState.h"
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
//  无状态约定：所有关卡进度（level / level_cleared / wave_*）统一存储在
//  GameState 单例组件（Entity 0）中，本系统不持有任何成员状态，只读写 GameState。
//
//  流水线位置：EnemyAISystem 之后、CollisionSystem 之前
//  （先刷怪，AI 再处理新敌人）
// =====================================================================

class LevelSystem : public System {
public:
    void update(World& world, float dt) override;

    // 加载指定关卡（1-based）；reset_hp=true 时重置玩家血量为满血（仅重新开始时使用）
    void load_level(World& world, int level_number, bool reset_hp = false);

    // 重置到第 1 关
    void reset(World& world);

    // 应用增益并进入下一关
    void apply_buff_and_next(World& world, BuffType buff);

private:
    void start_wave(World& world);
    void complete_level(World& world);
    void pick_buff_choices(World& world);

    // 由 gs->level 推导当前关卡在 levels 数组中的索引（0-based）
    int level_index(World& world) const;
};
