#pragma once
#include <cstdint>

// =====================================================================
//  元素领域的共享类型定义
// =====================================================================

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
// 注意：枚举值 = 位掩码 index，取值范围 0~15（刚好放进 uint16_t pending_reaction_mask）
//   将来扩反应（烈绽放/超绽放/激化等）继续 16、17…就超过 uint16，届时同步把
//   ElementStatus::pending_reaction_mask 与 RenderCommand 的 reaction_mask 字段
//   扩展为 uint32（lo/hi/2/3 共 4 字节，仍有 _pad 空间可复用）。
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
    SwirlPyro,             // 火扩散：风 + 火 → 扩散火范围伤害
    SwirlHydro,            // 水扩散：风 + 水 → 扩散水范围伤害
    SwirlElectro,          // 雷扩散：风 + 雷 → 扩散雷范围伤害
    SwirlCryo,             // 冰扩散：风 + 冰 → 扩散冰范围伤害
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
