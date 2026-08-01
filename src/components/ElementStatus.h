#pragma once
#include <cstdint>
#include <cmath>

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
    Count
};

// 元素反应类型
enum class ReactionType : uint8_t {
    None             = 0,  // 无反应
    Vaporize,              // 蒸发：水 + 火  → 水打火，倍率 ×1.5
    ReverseVaporize,       // 蒸发：火 + 水  → 火打水，倍率 ×2.0
    Melt,                  // 融化：火 + 冰  → 火打冰，倍率 ×2.0
    ReverseMelt,           // 融化：冰 + 火  → 冰打火，倍率 ×1.5
    Overload,              // 超载：火 + 雷  → 范围爆炸伤害 + 击飞
    ElectroCharged,        // 感电：雷 + 水  → 持续雷伤 + 传导
    Frozen,                // 冻结：冰 + 水  → 控制静止
    Superconduct,          // 超导：冰 + 雷  → 减物抗 + 范围冰伤
    Swirl,                 // 扩散：风 + 其他元素 → 扩散元素范围伤害
    Crystallize,           // 结晶：岩 + 其他元素 → 生成对应元素护盾
    Burning,               // 燃烧：火 + 草  → 持续火伤
    Bloom,                 // 绽放：水 + 草  → 生成草原核
    Quicken                // 激化：雷 + 草  → 增伤基底
};

// ==================== 元素强度分级 ====================
enum class ElementStrength : uint8_t {
    Weak          = 0,   // 1   理论 GU → 0.8 实际 → 9.5s
    Medium        = 1,   // 1.5 理论 GU → 1.2 实际 → 10.75s
    Strong        = 2,   // 2   理论 GU → 1.6 实际 → 12s
    ExtraStrong   = 3,   // 4   理论 GU → 3.2 实际 → 17s
    Count
};

// 元素强度参数（按用户给定表格 1:1 对应，包含 20% 附着瞬间折扣）
struct ElementStrengthParams {
    float theoretical_gauge;   // 理论 GU（子弹 payload、预设配置都用这个值）
    float actual_gauge;        // 真实初始附着量 = theoretical × 0.8
    float duration;            // 持续时间（秒，按原神非线性拟合值）
    float decay_rate;          // 每秒衰减速率 = actual_gauge / duration
};

// 根据强度等级查参数（最常用，敌人预设、初始化元素、子弹附着 —— 统一走枚举正向查表）
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

struct ElementStatus {
    ElementType type       = ElementType::None;   //当前附着的元素类型（None=无）
    float       gauge      = 0.0f;                //【实际附着元素量】= 理论量 × 0.8（附着瞬间立刻打八折）
    float       duration   = 0.0f;                // 附着总时长（秒），按强度分级查表
    float       decay_rate = 0.2f;                // 每秒衰减速率 【统一公式 = 实际gauge / duration，永远不再手填】

    ElementType     pending_element  = ElementType::None;     // 待处理的输入元素（碰撞命中的子弹元素写这里，本帧统一处理）
    ElementStrength pending_strength = ElementStrength::Weak; // 待处理的输入元素强度档（直接决定附着后的 gauge/duration/decay）

    ReactionType pending_reaction = ReactionType::None;  //待广播的反应脉冲（本帧触发反应时写入，OutputSystem 本帧读完立即清 0，只脉冲一帧）

    bool is_active()  const { return gauge > 0.0f && duration > 0.0f; }
    bool has_pending() const { return pending_element != ElementType::None; }

    void clear_pending() {
        pending_element  = ElementType::None;
        pending_strength = ElementStrength::Weak;
    }

    // 取走并清除待广播反应（OutputSystem 本帧消费一次，下一帧自动无）
    ReactionType consume_reaction() {
        ReactionType r = pending_reaction;
        pending_reaction = ReactionType::None;
        return r;
    }
};

// ===== 统一附着入口 =====
inline void attach_element(ElementStatus& s, ElementType type, ElementStrength strength) {
    s.type = type;
    ElementStrengthParams p = get_element_strength_params(strength);
    s.gauge      = p.actual_gauge;
    s.duration   = p.duration;
    s.decay_rate = p.decay_rate;
}

// 判断元素是否可以作为"附着状态"留在目标身上
//   可附着：Pyro / Hydro / Electro / Cryo / Dendro —— 这 5 种有附着光环 + 量条 + 自然衰减
//   不可附着（只能做后手触发元素）：Anemo 风 / Geo 岩 —— 命中反应后不残留附着
inline bool is_attachable_element(ElementType type) {
    return type == ElementType::Pyro   ||
           type == ElementType::Hydro  ||
           type == ElementType::Electro ||
           type == ElementType::Cryo   ||
           type == ElementType::Dendro;
}
