#pragma once

// =====================================================================
//  Health —— 血量组件
//
//  单一职责：仅存储实体的最大/当前血量。
//  伤害结算逻辑由 DamageSystem 统一处理（扣血、死亡判定、飘字）。
//  挂在：玩家、敌人（未来可扩展到任何可受伤实体）。
//
//  预留扩展点：
//    · 元素抗性：未来加 Resistance 组件，DamageSystem 扣血前查抗性减免
//    · 护盾：未来加 Shield 组件，DamageSystem 先扣护盾再扣血
// =====================================================================

struct Health {
    float max_hp     = 100.0f;  // 最大血量
    float current_hp = 100.0f;  // 当前血量（DamageSystem 扣减）

    // 死亡判定：current_hp 归零时由 DamageSystem 处理销毁
    bool is_dead() const { return current_hp <= 0.0f; }
};
