#pragma once

// =====================================================================
//  AttackCooldown —— 攻击节拍冷却组件
//
//  所有"周期性攻击/发射动作"实体共享的统一节拍组件：
//    · 玩家射击冷却
//    · 敌人近身攻击冷却
//    · duration  = 冷却时长（配置值，spawn 时填入，运行时只读）
//    · remaining = 剩余时间（状态值，<= 0 表示就绪）
//
//  配套系统 AttackCooldownSystem 统一递减（流水线最前），消费方只做：
//    查 remaining <= 0 → 触发 → remaining = duration * 动态倍率（可选）
//
//  命名说明：与 ElementICDStatus（元素命中 ICD）、Lifetime（存活）、
//  invincible_timer（无敌帧）等其他"计时"语义区分开——本组件只管
//  "攻击动作的触发节拍"。
// =====================================================================
struct AttackCooldown {
    float duration  = 0.0f;   // 冷却基础时长（秒），配置真源
    float remaining = 0.0f;   // 剩余时间（秒），<=0 表示就绪

    bool is_ready() const { return remaining <= 0.0f; }
};
