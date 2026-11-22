#pragma once

// =====================================================================
//  Cooldown —— 通用冷却组件
//
//  所有"可冷却"实体共享的统一节拍组件。
//    · duration  = 冷却时长（配置值，spawn 时填入，运行时只读）
//    · remaining = 剩余时间（状态值，<= 0 表示就绪）
//
//  配套系统 CooldownSystem 统一递减（流水线最前），消费方只做：
//    查 remaining <= 0 → 触发 → remaining = duration * 动态倍率（可选）
//
//  当前用途：
//    · 玩家射击冷却（取代 InputSystem::m_fire_cooldown 违规成员变量）
//    · 敌人攻击冷却（取代 EnemyAI::attack_timer + attack_cooldown）
//
//  模式可复用于未来任何需要"定时触发"的实体技能。
// =====================================================================
struct Cooldown {
    float duration  = 0.0f;   // 冷却基础时长（秒），配置真源
    float remaining = 0.0f;   // 剩余时间（秒），<=0 表示就绪

    bool is_ready() const { return remaining <= 0.0f; }
};
