#pragma once
#include "LevelConfig.h"
#include <vector>

// =====================================================================
//  Levels —— 内置关卡配置
//
//  第一阶段：基础关 + Boss 关交替，用于测试核心玩法。
//  后续可改为从 JSON 加载。
//
//  preset_id 约定：
//    0 = Pyro  火丘丘
//    1 = Hydro 水史莱姆
//    2 = Cryo  冰大怪
//    3 = Boss  （高血量大体型高攻击）
// =====================================================================

// 返回所有内置关卡配置
const std::vector<LevelConfig>& get_builtin_levels();
