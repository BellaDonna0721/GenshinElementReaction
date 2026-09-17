#pragma once
#include <cstdint>
#include "../utils/ElementType.h"

// =====================================================================
//  ElementICDStatus —— "元素命中冷却"组件（Internal Cooldown）
//
//  单一职责：
//    · 每个目标实体（玩家/敌人）独立持有 7(元素) × 2(档位) = 14 组
//      {timer, counter}，控制同一元素+档位攻击的附着频率。
//    · 与 ElementStatus（光环）完全正交：
//        - ElementStatus 关心"目标身上挂了什么元素"
//        - ElementICDStatus 关心"这次命中要不要放行"
//      两者通过 World 的 query<ElementStatus, ElementICDStatus>() 在
//      系统层组合，组件本身互不依赖（都只依赖 ElementType.h）。
//
//  规则：
//    · 分组 key = (触发元素类型 × 攻击档位) 二元组 —— 彼此完全独立
//      - 7 种元素：Pyro/Hydro/Electro/Cryo/Anemo/Geo/Dendro（0-6 下标，含风岩）
//      - 2 种档位：Weak/Medium → 档 0；Strong/ExtraStrong → 档 1
//      → 每实体 7×2=14 组独立 {timer, counter}
//    · 每组各自：
//      · timer==0 停表；首次命中立刻启动新一轮（timer=2.5s, counter=0）
//      · 每次命中 counter += 1（拦截也计入）
//      · 放行条件 counter%3==1 → 序列 1,4,7,10…
//      · 周期到点 timer<=0 → counter=0 且 timer 归零停表，下次命中重启新一轮
//    · 命中若被 ICD 拦截：既不挂元素也不触发反应
// =====================================================================
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

// ==================== ICD 组件 API（操作 ICD 数据的领域函数）====================
// 设计说明：
//   这两个函数只读写 ElementICDStatus 自身的数据（外加传入的触发元素/档位参数），
//   不触碰 ElementStatus，因此可以安全地放在本组件头里作为"组件 API"。
//   真正的每帧调度由 ElementReactionSystem（decay）和 CollisionSystem（hit_allowed）负责。

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
