#pragma once
#include <cstdint>
#include <vector>

// =====================================================================
//  DamageNumberQueue —— 伤害飘字队列（单例组件，挂 Entity 0）
//
//  写入方：DamageSystem（每次扣血时 push 一条）
//  消费方：OutputSystem（读取并写入全局 g_damage_numbers 供 TS 渲染）
//
//  TS 侧负责飘字的渐隐 + 上浮动画（复用反应飘字的客户端缓存模式）。
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
