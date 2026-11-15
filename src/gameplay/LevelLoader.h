#pragma once
#include "LevelConfig.h"
#include <string>
#include <vector>

// =====================================================================
//  LevelLoader —— 关卡数据的加载、存储与访问
//
//  关卡数据由 wasm_load_levels 从 JSON 加载后填充（parse_levels_json），
//  LevelSystem 通过 get_levels() 读取。
//
//  preset_id 约定：
//    0 = Pyro  火丘丘
//    1 = Hydro 水史莱姆
//    2 = Cryo  冰大怪
//    3 = Boss  （高血量大体型高攻击）
// =====================================================================

// 从 JSON 字符串解析关卡列表（归一化坐标 → 像素）
std::vector<LevelConfig> parse_levels_json(const std::string& text);

// 返回当前关卡列表（只读）
const std::vector<LevelConfig>& get_levels();

// 用解析后的关卡列表替换全局数据
void set_levels(std::vector<LevelConfig> levels);
