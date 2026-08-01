#include "ElementReactionSystem.h"
#include <cstdio>

ElementReactionSystem::ElementReactionSystem() {
    // 约定：
    //   consumes_target  = true  → 允许消耗目标身上的元素（精细计算后决定全清 or 部分扣减，不是直接暴力清）
    //   consumes_trigger = true  → 后手元素不残留（任何反应命中后，触发元素都不再附着到敌人身上）
    //   target_cost_per_trigger  → 每 1 单位【触发元素实际量】能吃多少【目标元素实际量】
    //     - 克制方向（水→火、火→冰）= 2.0：1 单位触发能顶 2 单位目标
    //     - 被克制方向（火→水、冰→火）= 0.5：1 单位触发只能顶 0.5 单位目标

    // --- 蒸发 Vaporize：水 ↔ 火 ---
    // 水→火（正向蒸发 水克火 → 1 水消耗 2 火）
    m_rules[m_rule_count++] = {
        ElementType::Hydro, ElementType::Pyro,
        ReactionType::Vaporize,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 2.0f
    };
    // 火→水（反向蒸发 火被水克 → 1 火只能消耗 0.5 水）
    m_rules[m_rule_count++] = {
        ElementType::Pyro, ElementType::Hydro,
        ReactionType::ReverseVaporize,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 0.5f
    };

    // --- 融化 Melt：火 ↔ 冰 ---
    // 火→冰（正向融化 火克冰 → 1 火消耗 2 冰）
    m_rules[m_rule_count++] = {
        ElementType::Pyro, ElementType::Cryo,
        ReactionType::Melt,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 2.0f
    };
    // 冰→火（反向融化 ×1.5，冰被火克 → 1 冰只能消耗 0.5 火）
    m_rules[m_rule_count++] = {
        ElementType::Cryo, ElementType::Pyro,
        ReactionType::ReverseMelt,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 0.5f
    };

    // --- 超载 Overload：火 ↔ 雷 ---
    m_rules[m_rule_count++] = {
        ElementType::Pyro, ElementType::Electro,
        ReactionType::Overload,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 1.0f
    };

    m_rules[m_rule_count++] = {
        ElementType::Electro, ElementType::Pyro,
        ReactionType::Overload,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 1.0f
    };

    //--- 感电 ElectroCharged：雷 ↔ 水 --- 
    //后续完善水雷共存
    m_rules[m_rule_count++] = {
        ElementType::Electro, ElementType::Hydro,
        ReactionType::ElectroCharged,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 1.0f
    };

    m_rules[m_rule_count++] = {
        ElementType::Hydro, ElementType::Electro,
        ReactionType::ElectroCharged,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 1.0f
    };

    //--- 冰冻 Frozen：冰 ↔ 水 ---
    //后续完善冻元素
    m_rules[m_rule_count++] = {
        ElementType::Cryo, ElementType::Hydro,
        ReactionType::Frozen,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 1.0f
    };

    m_rules[m_rule_count++] = {
        ElementType::Hydro, ElementType::Cryo,
        ReactionType::Frozen,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 1.0f
    };

    //--- 超导 Superconduct：冰 ↔ 雷 ---
    m_rules[m_rule_count++] = {
        ElementType::Cryo, ElementType::Electro,
        ReactionType::Superconduct,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 1.0f
    };

    m_rules[m_rule_count++] = {
        ElementType::Electro, ElementType::Cryo,
        ReactionType::Superconduct,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 1.0f
    };

    // --- 扩散 Swirl：风 → 火 / 水 / 雷 / 冰
    m_rules[m_rule_count++] = {
        ElementType::Anemo, ElementType::Pyro,
        ReactionType::Swirl,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 1.0f
    };
    m_rules[m_rule_count++] = {
        ElementType::Anemo, ElementType::Hydro,
        ReactionType::Swirl,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 1.0f
    };
    m_rules[m_rule_count++] = {
        ElementType::Anemo, ElementType::Electro,
        ReactionType::Swirl,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 1.0f
    };
    m_rules[m_rule_count++] = {
        ElementType::Anemo, ElementType::Cryo,
        ReactionType::Swirl,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 1.0f
    };

    // --- 结晶 Crystallize：岩 → 火 / 水 / 雷 / 冰（Geo 同样仅作后手触发元素，不可附着）
    m_rules[m_rule_count++] = {
        ElementType::Geo, ElementType::Pyro,
        ReactionType::Crystallize,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 1.0f
    };
    m_rules[m_rule_count++] = {
        ElementType::Geo, ElementType::Hydro,
        ReactionType::Crystallize,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 1.0f
    };
    m_rules[m_rule_count++] = {
        ElementType::Geo, ElementType::Electro,
        ReactionType::Crystallize,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 1.0f
    };
    m_rules[m_rule_count++] = {
        ElementType::Geo, ElementType::Cryo,
        ReactionType::Crystallize,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 1.0f
    };

    // --- 燃烧 Burning：火 ↔ 草（双向，Dendro 可附着，正反都可触发）
    //后续完善
    m_rules[m_rule_count++] = {
        ElementType::Dendro, ElementType::Pyro,
        ReactionType::Burning,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 1.0f
    };
    m_rules[m_rule_count++] = {
        ElementType::Pyro, ElementType::Dendro,
        ReactionType::Burning,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 1.0f
    };

    // --- 绽放 Bloom：水 ↔ 草（双向，Dendro 可附着，正反都可触发）
    m_rules[m_rule_count++] = {
        ElementType::Dendro, ElementType::Hydro,
        ReactionType::Bloom,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 1.0f
    };
    m_rules[m_rule_count++] = {
        ElementType::Hydro, ElementType::Dendro,
        ReactionType::Bloom,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 1.0f
    };

    // --- 激化 Quicken：雷 ↔ 草（双向 1:1，Dendro 可附着，正反都可触发）
    m_rules[m_rule_count++] = {
        ElementType::Electro, ElementType::Dendro,
        ReactionType::Quicken,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 1.0f
    };
    m_rules[m_rule_count++] = {
        ElementType::Dendro, ElementType::Electro,
        ReactionType::Quicken,
        /*consumes_target*/ true, /*consumes_trigger*/ true,
        /*target_cost_per_trigger*/ 1.0f
    };
}

const ReactionRule* ElementReactionSystem::find_rule(
    ElementType trigger, ElementType target) const
{
    for (int i = 0; i < m_rule_count; ++i) {
        if (m_rules[i].trigger == trigger && m_rules[i].target == target) {
            return &m_rules[i];
        }
    }
    return nullptr;
}

void ElementReactionSystem::update(World& world, float dt) {
    for (auto e : world.query<ElementStatus>()) {
        auto* elem = world.get_component<ElementStatus>(e);
        if (!elem) continue;

        // ① 已附着元素：线性衰减 gauge/duration，到 0 就清类型
        if (elem->is_active()) {
            // 保险兜底：如果身上意外挂了 Anemo/Geo 这种"不可附着元素"，立即清掉
            if (!is_attachable_element(elem->type)) {
                elem->type     = ElementType::None;
                elem->gauge    = 0.0f;
                elem->duration = 0.0f;
            } else {
                elem->duration -= dt;
                elem->gauge    -= elem->decay_rate * dt;
                if (elem->gauge <= 0.0f || elem->duration <= 0.0f) {
                    elem->type     = ElementType::None;
                    elem->gauge    = 0.0f;
                    elem->duration = 0.0f;
                }
            }
        }

        // ② 有待处理的命中元素（碰撞系统写入的子弹元素）
        if (elem->has_pending()) {
            const ReactionRule* rule = nullptr;
            if (elem->is_active()) {
                rule = find_rule(elem->pending_element, elem->type);
            }

            if (rule) {
                // ========== 命中反应 ==========
                // 1. 广播 pending_reaction 脉冲（TS 侧拿 reaction_type 启动小字渐隐）
                elem->pending_reaction = rule->result;
                const char* rname = "未知反应";
                switch (rule->result) {
                    case ReactionType::Vaporize:        rname = "蒸发(水→火)"; break;
                    case ReactionType::ReverseVaporize: rname = "蒸发(火→水)"; break;
                    case ReactionType::Melt:            rname = "融化(火→冰)"; break;
                    case ReactionType::ReverseMelt:     rname = "融化(冰→火)"; break;
                    case ReactionType::Overload:        rname = "超载(火↔雷)"; break;
                    case ReactionType::ElectroCharged:  rname = "感电(雷↔水)"; break;
                    case ReactionType::Frozen:          rname = "冻结(冰↔水)"; break;
                    case ReactionType::Superconduct:    rname = "超导(冰↔雷)"; break;
                    case ReactionType::Swirl:           rname = "扩散(风→元素)"; break;
                    case ReactionType::Crystallize:     rname = "结晶(岩→元素)"; break;
                    case ReactionType::Burning:         rname = "燃烧(火↔草)"; break;
                    case ReactionType::Bloom:           rname = "绽放(水↔草)"; break;
                    case ReactionType::Quicken:         rname = "激化(雷↔草)"; break;
                    default: break;
                }
                float trigger_actual = get_element_strength_params(elem->pending_strength).actual_gauge;
                float target_cost    = trigger_actual * rule->target_cost_per_trigger;
                printf("[Reaction] Entity %u: %s  |  trigger_actual=%.2f  ratio=%.2f  → 消耗目标=%.2f  反应前目标剩余=%.2f\n", e, rname,
                    trigger_actual, rule->target_cost_per_trigger, target_cost, elem->gauge);

                // 2. 处理目标元素（敌人身上原来附着的元素）消耗
                //    【精细规则】不再按 bool 一刀切全清，而是按"1 触发实际量 × 克制比值 → 能吃多少目标量"计算
                //      - 若能吃的量 >= 目标当前 gauge  → 目标吃光（type/gauge/duration 全清）
                //      - 若能吃的量 <  目标当前 gauge  → 目标只扣 gauge，type/duration 保留（元素光环继续存在但量变少）
                if (rule->consumes_target) {
                    if (target_cost >= elem->gauge) {
                        elem->type     = ElementType::None;
                        elem->gauge    = 0.0f;
                        elem->duration = 0.0f;
                    } else {
                        elem->gauge -= target_cost;
                    }
                }

                // 3. 处理触发元素（子弹带来的新元素）残留
                //    用户规则：后手元素不残留（4 条反应规则 consumes_trigger 都为 true，所以这里永远不残留）
                //    保留通用结构不变：consumes_trigger=false 的反应（未来加的）才讨论残留
                if (rule->consumes_target && !rule->consumes_trigger) {
                    if (is_attachable_element(elem->pending_element)) {
                        attach_element(*elem, elem->pending_element, elem->pending_strength);
                    }
                }
            } else {
                // ========== 无反应（直接附着）==========
                // 元素类型不匹配反应表，或身上本来就没元素 → 触发元素覆盖附着
                // 限制 1：Anemo/Geo 是后手触发专用，永远不允许挂为附着元素
                // 限制 2：附着瞬间按强度档查标准表（×0.8 + 对应 duration/decay）
                if (elem->pending_element != ElementType::None
                        && is_attachable_element(elem->pending_element)) {
                    attach_element(*elem, elem->pending_element, elem->pending_strength);
                }
            }

            // 本帧的 pending 元素/量/反应消息全部处理完，清干净防下一帧重复消费
            elem->clear_pending();
        }
    }
}