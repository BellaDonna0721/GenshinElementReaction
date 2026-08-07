#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>

// 元素类型（七元素 + 空）
enum class ElementType : uint8_t {
    None    = 0,  // 无元素
    Pyro,         // 火元素
    Hydro,        // 水元素
    Electro,      // 雷元素
    Cryo,         // 冰元素
    Anemo,        // 风元素
    Geo,          // 岩元素
    Dendro,       // 草元素
};

// 元素反应类型
enum class ReactionType : uint8_t {
    None             = 0,  // 无反应
    Vaporize,              // 蒸发：水 + 火  → 水打火，倍率 ×1.5
    ReverseVaporize,       // 蒸发：火 + 水  → 火打水，倍率 ×2.0
    Melt,                  // 融化：火 + 冰  → 火打冰，倍率 ×2.0
    ReverseMelt,           // 融化：冰 + 火  → 冰打火，倍率 ×1.5
    Overload,              // 超载：火 + 雷  → 范围爆炸伤害 + 击飞
    ElectroCharged,        // 感电：雷 + 水  → 共存态 + 每 1s 各扣 0.4 DoT
    Frozen,                // 冻结：冰 + 水  → 控制静止
    Superconduct,          // 超导：冰 + 雷  → 减物抗 + 范围冰伤
    Swirl,                 // 扩散：风 + 其他元素 → 扩散元素范围伤害
    Crystallize,           // 结晶：岩 + 其他元素 → 生成对应元素护盾
    Burning,               // 燃烧：火 + 草  → 持续火伤
    Bloom,                 // 绽放：水 + 草  → 生成草原核
};

// ==================== 元素强度分级 ====================
enum class ElementStrength : uint8_t {
    Weak          = 0,   // 1   理论 GU → 0.8 实际 → 9.5s
    Medium        = 1,   // 1.5 理论 GU → 1.2 实际 → 10.75s
    Strong        = 2,   // 2   理论 GU → 1.6 实际 → 12s
    ExtraStrong   = 3,   // 4   理论 GU → 3.2 实际 → 17s
};

// 元素强度参数（按用户给定表格 1:1 对应，包含 20% 附着瞬间折扣）
struct ElementStrengthParams {
    float theoretical_gauge;   // 理论 GU（子弹 payload、预设配置都用这个值）
    float actual_gauge;        // 真实初始附着量 = theoretical × 0.8
    float duration;            // 持续时间（秒，按原神非线性拟合值）
    float decay_rate;          // 每秒衰减速率 = actual_gauge / duration
};

// 根据强度等级查参数
inline ElementStrengthParams get_element_strength_params(ElementStrength s) {
    ElementStrengthParams p = {};
    switch (s) {
        case ElementStrength::Weak:
            p.theoretical_gauge = 1.0f;  p.actual_gauge = 0.8f;
            p.duration = 9.5f;   p.decay_rate = p.actual_gauge / p.duration;
            return p;
        case ElementStrength::Medium:
            p.theoretical_gauge = 1.5f;  p.actual_gauge = 1.2f;
            p.duration = 10.75f; p.decay_rate = p.actual_gauge / p.duration;
            return p;
        case ElementStrength::Strong:
            p.theoretical_gauge = 2.0f;  p.actual_gauge = 1.6f;
            p.duration = 12.0f;  p.decay_rate = p.actual_gauge / p.duration;
            return p;
        case ElementStrength::ExtraStrong:
            p.theoretical_gauge = 4.0f;  p.actual_gauge = 3.2f;
            p.duration = 17.0f;  p.decay_rate = p.actual_gauge / p.duration;
            return p;
        default:
            return p;
    }
}

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
    // —— 玩家专用：当前选中的射击元素（敌人永远为 None）——
    // 与 slots[] 附着光环完全解耦：玩家身上可以同时"持有"火元素射击能力
    // 并且"被附着"了水+雷共存光环，两者互不干扰。
    ElementType current_player_elem = ElementType::None;

    // —— 附着元素（双槽，最多两种共存；水雷相遇即进入该态）——
    AttachedSlot slots[2];

    // 水雷共存专用：距离下一次感电 DoT 扣 0.4/0.4 的剩余秒数
    // 只有同时存在 Hydro + Electro 时才 >0；其他情况自动为 0
    float electrocharged_cooldown = 0.0f;

    // —— 待处理命中元素（碰撞写，本帧统一处理）——
    ElementType     pending_element  = ElementType::None;
    ElementStrength pending_strength = ElementStrength::Weak;

    // —— 反应脉冲（OutputSystem 每帧消费一次）——
    ReactionType pending_reaction = ReactionType::None;

    // =============== 查询工具 ===============
    // 至少一个槽位有元素 → 视为 active（敌人身上有 aura 环）
    bool is_active() const { return slots[0].is_valid() || slots[1].is_valid(); }

    bool has_pending() const { return pending_element != ElementType::None; }

    int attached_count() const {
        return (slots[0].is_valid() ? 1 : 0) + (slots[1].is_valid() ? 1 : 0);
    }

    // 返回指向 slot 的指针（方便外部读写），找不到返回 nullptr
    AttachedSlot* find_slot(ElementType t) {
        if (slots[0].type == t && slots[0].is_valid()) return &slots[0];
        if (slots[1].type == t && slots[1].is_valid()) return &slots[1];
        return nullptr;
    }
    const AttachedSlot* find_slot(ElementType t) const {
        return const_cast<ElementStatus*>(this)->find_slot(t);
    }

    // 返回第一个空槽（或 nullptr 表示两槽都满）
    AttachedSlot* find_empty_slot() {
        if (!slots[0].is_valid()) return &slots[0];
        if (!slots[1].is_valid()) return &slots[1];
        return nullptr;
    }

    // =============== 本帧末尾清理 ===============
    void clear_pending() {
        pending_element  = ElementType::None;
        pending_strength = ElementStrength::Weak;
    }

    // 消费一次性反应脉冲（OutputSystem 调用）
    ReactionType consume_reaction() {
        ReactionType r = pending_reaction;
        pending_reaction = ReactionType::None;
        return r;
    }
};

// ===== 统一附着入口（兼容旧调用点：找空 slot 写入；没空就覆盖 slot[0]）=====
inline void attach_element(ElementStatus& s, ElementType type, ElementStrength strength) {
    ElementStrengthParams p = get_element_strength_params(strength);
    AttachedSlot* target = s.find_empty_slot();
    if (!target) target = &s.slots[0];  // 兜底：没有空槽就覆盖第一个（一般不会发生，共存最多 Hydro+Electro=2 种）
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
