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
// 注意：枚举值 = 位掩码 index，取值范围 0~15（刚好放进 uint16_t pending_reaction_mask）
//   将来扩反应（烈绽放/超绽放/激化等）继续 16、17…就超过 uint16，届时同步把 ElementStatus::pending_reaction_mask
//   与 RenderCommand 的 reaction_mask 字段扩展为 uint32（lo/hi/2/3 共 4 字节，仍有 _pad 空间可复用）。
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

// ==================== 元素附着 ICD（Internal Cooldown）====================
// 规则：
//   · 分组 key = (触发元素类型 × 攻击档位) 二元组 —— 彼此完全独立（新修复规格）
//     - 7 种元素：Pyro/Hydro/Electro/Cryo/Anemo/Geo/Dendro（0-6 下标，含风岩）
//     - 2 种档位：Weak/Medium → 档 0；Strong/ExtraStrong → 档 1
//     → 每实体 7×2=14 组独立 {timer, counter}
//   · 每组各自：
//     · timer==0 停表；首次命中立刻启动新一轮（timer=2.5s, counter=0）规则⑥
//     · 每次命中 counter += 1（拦截也计入）
//     · 放行条件 counter%3==1 → 序列 1,4,7,10…（规则②③合并）
//     · 周期到点 timer<=0 → counter=0 且 timer 归零停表，下次命中重启新一轮
//   · 每个目标实体（玩家/敌人）独立持有这 14 组（规则⑤）
//   · 命中若被 ICD 拦截：既不挂元素也不触发反应（用户选 B 规则①）
struct ElementICDStatus {
    // None=0, Pyro=1..Dendro=7 → 直接用枚举值当下标
    static constexpr int ELEM_DIM = 8;
    static constexpr int GEAR_DIM = 2;  // 2 档（Weak=0 / Strong=1）

    struct CounterSlot {
        float timer   = 0.0f;
        int   counter = 0;
    };
    CounterSlot icd_counters[ELEM_DIM][GEAR_DIM];  // [直接枚举值][gear]

    static constexpr float CYCLE_SECONDS = 2.5f;

};

// ==================== ICD 公开工具函数 ====================
// ① 每帧衰减：从 1..7（None=0 不用循环）
inline void icd_decay_tick(ElementICDStatus& icd, float dt) {
    for (int ei = 1; ei < ElementICDStatus::ELEM_DIM; ++ei) {
        for (int gi = 0; gi < ElementICDStatus::GEAR_DIM; ++gi) {
            ElementICDStatus::CounterSlot& s = icd.icd_counters[ei][gi];
            if (s.timer > 0.0f) {
                s.timer -= dt;
                if (s.timer <= 0.0f) {
                    s.timer   = 0.0f;
                    s.counter = 0;
                }
            }
        }
    }
}

// ② 命中判定
//   - ElementType::None（ei=0）→ 直接放行（ICD 不约束无元素的攻击）
inline bool icd_hit_allowed(ElementICDStatus& icd, ElementType trigger, ElementStrength strength) {
    int ei = static_cast<int>(trigger);
    if (ei <= 0 || ei >= ElementICDStatus::ELEM_DIM) return true;
    int gi = (strength == ElementStrength::Strong) ? 1 : 0;

    ElementICDStatus::CounterSlot& s = icd.icd_counters[ei][gi];

    // 新一轮启动
    if (s.timer <= 0.0f) {
        s.timer   = ElementICDStatus::CYCLE_SECONDS;
        s.counter = 0;
    }

    s.counter += 1;
    return (s.counter % 3 == 1);
}
