#pragma once
#include <cstdint>
#include <algorithm>
#include "../utils/ElementType.h"

// =====================================================================
//  ElementStatus —— "元素附着光环"组件（只关心目标身上挂了什么元素）
//
//  单一职责：
//    · 双槽附着元素（水雷共存用）的 gauge/duration 数据
//    · 待处理命中元素（碰撞写入，本帧统一结算）
//    · 反应脉冲位掩码（OutputSystem 消费后渲染飘字）
//
//  不再承载 ICD 命中冷却（见 ElementICDStatus.h）—— 两者是正交关注点：
//  改光环规则只动本文件，改 ICD 规则只动 ElementICDStatus.h。
// =====================================================================

// 单个附着元素槽
struct AttachedSlot {
    ElementType type          = ElementType::None;
    float       gauge         = 0.0f;   // 当前量（随时间衰减）
    float       initial_gauge = 0.0f;   // 附着瞬间的初始量（用于量条比例：gauge/initial_gauge = 0~100%）
    float       duration      = 0.0f;
    float       decay_rate    = 0.2f;

    bool is_valid() const { return type != ElementType::None && gauge > 0.0f && duration > 0.0f; }

    void clear() {
        type          = ElementType::None;
        gauge         = 0.0f;
        initial_gauge = 0.0f;
        duration      = 0.0f;
    }
};

struct ElementStatus {
    // —— 附着元素（双槽，最多两种共存；水雷相遇即进入该态）——
    AttachedSlot attached_slots[2];

    // 水雷共存专用：距离下一次感电 DoT 扣 0.4/0.4 的剩余秒数
    // 只有同时存在 Hydro + Electro 时才 >0；其他情况自动为 0
    float electrocharged_cooldown = 0.0f;

    // —— 待处理命中元素（碰撞写，本帧统一处理）——
    ElementType     pending_element  = ElementType::None;
    ElementStrength pending_strength = ElementStrength::Weak;

    // —— 反应脉冲位掩码（OutputSystem 每帧消费一次，16 位最多同时标记 16 种反应）
    //   位 index = static_cast<int>(ReactionType)：
    //     bit 0 = None（始终 0，无用）
    //     bit 1 = Vaporize, bit 2 = ReverseVaporize, bit 3 = Melt, ...
    //     bit 13 = Bloom（当前上限）
    //   同帧发生的多个反应通过 OR 叠加（阶段 2 感电 DoT 与阶段 3 新触发反应不再互相覆盖）。
    uint16_t pending_reaction_mask = 0;

    // =============== 查询工具 ===============
    // 至少一个槽位有元素 → 视为 active（敌人身上有 aura 环）
    bool is_active() const { return attached_slots[0].is_valid() || attached_slots[1].is_valid(); }

    bool has_pending() const { return pending_element != ElementType::None; }

    int attached_count() const {
        return (attached_slots[0].is_valid() ? 1 : 0) + (attached_slots[1].is_valid() ? 1 : 0);
    }

    // 返回指向 slot 的指针（方便外部读写），找不到返回 nullptr
    AttachedSlot* find_slot(ElementType t) {
        if (attached_slots[0].type == t && attached_slots[0].is_valid()) return &attached_slots[0];
        if (attached_slots[1].type == t && attached_slots[1].is_valid()) return &attached_slots[1];
        return nullptr;
    }
    const AttachedSlot* find_slot(ElementType t) const {
        return const_cast<ElementStatus*>(this)->find_slot(t);
    }

    // 返回第一个空槽（或 nullptr 表示两槽都满）
    AttachedSlot* find_empty_slot() {
        if (!attached_slots[0].is_valid()) return &attached_slots[0];
        if (!attached_slots[1].is_valid()) return &attached_slots[1];
        return nullptr;
    }

    // =============== 写工具 ===============
    void clear_pending() {
        pending_element  = ElementType::None;
        pending_strength = ElementStrength::Weak;
    }

    // 标记一个反应发生过（OR 进位掩码；同反应重复标记不叠加）
    void mark_reaction(ReactionType rt) {
        int idx = static_cast<int>(rt);
        if (idx > 0 && idx < 16) pending_reaction_mask |= static_cast<uint16_t>(1u << idx);
    }

    // 消费所有本帧标记过的反应脉冲（返回位掩码 + 清零），供 OutputSystem 渲染飘字
    uint16_t consume_reactions() {
        uint16_t m = pending_reaction_mask;
        pending_reaction_mask = 0;
        return m;
    }
};

// ===== 统一附着入口（兼容旧调用点：找空 slot 写入；没空就覆盖 attached_slots[0]）=====
inline void attach_element(ElementStatus& s, ElementType type, ElementStrength strength) {
    ElementStrengthParams p = get_element_strength_params(strength);
    AttachedSlot* target = s.find_empty_slot();
    if (!target) target = &s.attached_slots[0];  // 兜底：没有空槽就覆盖第一个（一般不会发生，共存最多 Hydro+Electro=2 种）
    target->type          = type;
    target->gauge         = p.actual_gauge;
    target->initial_gauge = p.actual_gauge;   // 初始基准：量条比例 = gauge/initial_gauge
    target->duration      = p.duration;
    target->decay_rate    = p.decay_rate;
}

// 判断元素是否可以作为"附着状态"留在目标身上
//   可附着：Pyro / Hydro / Electro / Cryo / Dendro
//   不可附着（只能做后手触发元素）：Anemo / Geo
inline bool is_attachable_element(ElementType type) {
    return type == ElementType::Pyro    ||
           type == ElementType::Hydro   ||
           type == ElementType::Electro ||
           type == ElementType::Cryo    ||
           type == ElementType::Dendro;
}
