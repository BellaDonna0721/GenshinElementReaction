#pragma once
#include <cstdint>
#include <vector>

// =====================================================================
//  DamageNumberQueue —— 伤害飘字队列（单例组件，挂 Entity 0）
//
//  写入方：DamageSystem（帧首清空，扣血时 push 一条）
//  消费方：wasm_api 直接读取 numbers vector 供 TS 渲染
//
//  生命周期由写入方管理：DamageSystem::update 开头 clear，本帧写入的飘字
//  留存至下一帧 DamageSystem 清空之前，TS 在系统管线结束后读取。
// =====================================================================

// 单条伤害飘字数据
struct DamageNumber {
    float   x, y;       // 出现位置（怪物坐标；TS 侧再加偏移上浮）
    int     amount;     // 整数伤害（DamageSystem 中 round 后写入）
    uint8_t r, g, b;    // 元素颜色（DamageSystem 中查 get_element_color）
    uint8_t is_transformative = 0;  // 1=剧变反应伤害（小字号右侧），0=直伤/增幅（大字号）
};

// 单例组件：本帧产生的所有伤害飘字
struct DamageNumberQueue {
    std::vector<DamageNumber> numbers;
};
