#pragma once
#include "../components/Transform.h"
#include <vector>

// =====================================================================
//  LevelConfig —— 关卡配置（纯数据结构）
//
//  描述一个关卡的全部静态内容：
//    · 墙体布局
//    · 玩家出生点
//    · 波次刷怪配置（按顺序执行）
//    · 是否为 Boss 关（Boss 关通关后才给增益选项）
//
//  关卡数据可由 JSON 加载或 C++ 硬编码内置。
//  LevelSystem 消费此结构完成刷怪与通关判定。
// =====================================================================

// 单个怪物生成配置
struct EnemySpawn {
    int   preset_id;   // 敌人预设 ID（对应 wasm_spawn_enemy 的 preset_id）
    float x;           // 生成位置 X
    float y;           // 生成位置 Y
    int   count;       // 生成数量
    float interval;    // 同批生成间隔（秒）
};

// 单个波次配置
struct WaveConfig {
    float                delay;     // 波次开始前延迟（秒）
    std::vector<EnemySpawn> spawns; // 本波所有生成点
};

// 关卡配置
struct LevelConfig {
    int                            level_id;     // 关卡编号
    bool                           is_boss;      // 是否 Boss 关（Boss 关通关给增益）
    Transform                      player_spawn; // 玩家出生点
    std::vector<struct WallDef>    walls;        // 墙体列表
    std::vector<WaveConfig>        waves;        // 波次列表（按顺序执行）
};

// 墙体定义（独立于 ECS 组件，仅用于关卡数据描述）
struct WallDef {
    float x, y;       // 中心坐标
    float w, h;       // 宽高
};
