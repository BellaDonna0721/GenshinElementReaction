#include "ElementReactionSystem.h"
#include <cstdio>
#include <utility>

// 构造函数：注册所有元素反应规则
ElementReactionSystem::ElementReactionSystem() {
    auto add_rule = [this](
        ElementType trigger, ElementType target, ReactionType result,
        bool consumes_target, bool consumes_trigger, float target_cost_per_trigger
    ) {
        ReactionRule rule;
        rule.trigger               = trigger;
        rule.target                = target;
        rule.result                = result;
        rule.consumes_target       = consumes_target;
        rule.consumes_trigger      = consumes_trigger;
        rule.target_cost_per_trigger = target_cost_per_trigger;
        m_rule_map.insert_or_assign(
            std::make_pair(trigger, target),
            rule
        );
    };

    // 蒸发
    add_rule(ElementType::Hydro,  ElementType::Pyro,   ReactionType::Vaporize,         true,true, 2.0f);
    add_rule(ElementType::Pyro,   ElementType::Hydro,  ReactionType::ReverseVaporize,  true,true, 0.5f);
    // 融化
    add_rule(ElementType::Pyro,   ElementType::Cryo,   ReactionType::Melt,             true,true, 2.0f);
    add_rule(ElementType::Cryo,   ElementType::Pyro,   ReactionType::ReverseMelt,      true,true, 0.5f);
    // 超载
    add_rule(ElementType::Pyro,   ElementType::Electro,ReactionType::Overload,         true,true, 1.0f);
    add_rule(ElementType::Electro,ElementType::Pyro,   ReactionType::Overload,         true,true, 1.0f);

    // 感电反应（水↔雷）→ 进入共存态
    // consumes_target/trigger 均 false：不立即扣量，目标保留原光环，新元素写入第二槽；
    // 实际消耗量由后续每秒 DoT 循环统一处理（每次各扣 0.4）。
    add_rule(ElementType::Electro, ElementType::Hydro,  ReactionType::ElectroCharged,  false,false, 1.0f);
    add_rule(ElementType::Hydro,   ElementType::Electro,ReactionType::ElectroCharged,  false,false, 1.0f);

    // 冻结
    add_rule(ElementType::Cryo,   ElementType::Hydro,  ReactionType::Frozen,           true,true, 1.0f);
    add_rule(ElementType::Hydro,  ElementType::Cryo,   ReactionType::Frozen,           true,true, 1.0f);
    // 超导
    add_rule(ElementType::Cryo,   ElementType::Electro,ReactionType::Superconduct,     true,true, 1.0f);
    add_rule(ElementType::Electro,ElementType::Cryo,   ReactionType::Superconduct,     true,true, 1.0f);
    // 扩散（风 × 4，每种元素一个独立 SwirlXxx → 位掩码可同时标记多个）
    // 关键：consumes_trigger = false → 触发方（Anemo）不被按比例扣除 remaining，
    //       这样水雷共存目标先 k=0 扩散雷后，仍有足够 remaining 继续 k=1 扩散水，
    //       最终两个扩散都会被 mark_reaction → 同时飘字"雷扩散 + 水扩散"。
    add_rule(ElementType::Anemo,  ElementType::Pyro,   ReactionType::SwirlPyro,        true,false, 1.0f);
    add_rule(ElementType::Anemo,  ElementType::Hydro,  ReactionType::SwirlHydro,       true,false, 1.0f);
    add_rule(ElementType::Anemo,  ElementType::Electro,ReactionType::SwirlElectro,     true,false, 1.0f);
    add_rule(ElementType::Anemo,  ElementType::Cryo,   ReactionType::SwirlCryo,        true,false, 1.0f);
    // 结晶（岩 × 4）
    add_rule(ElementType::Geo,    ElementType::Pyro,   ReactionType::Crystallize,      true,true, 1.0f);
    add_rule(ElementType::Geo,    ElementType::Hydro,  ReactionType::Crystallize,      true,true, 1.0f);
    add_rule(ElementType::Geo,    ElementType::Electro,ReactionType::Crystallize,      true,true, 1.0f);
    add_rule(ElementType::Geo,    ElementType::Cryo,   ReactionType::Crystallize,      true,true, 1.0f);
    // 燃烧
    add_rule(ElementType::Dendro, ElementType::Pyro,   ReactionType::Burning,          true,true, 1.0f);
    add_rule(ElementType::Pyro,   ElementType::Dendro, ReactionType::Burning,          true,true, 1.0f);
    // 绽放
    add_rule(ElementType::Dendro, ElementType::Hydro,  ReactionType::Bloom,            true,true, 1.0f);
    add_rule(ElementType::Hydro,  ElementType::Dendro, ReactionType::Bloom,            true,true, 1.0f);
}

// 基于 myUnorderedMap 的 O(1) 查表
const ReactionRule* ElementReactionSystem::find_rule(
    ElementType trigger, ElementType target) const
{
    auto* p = m_rule_map.find(std::make_pair(trigger, target));
    return p ? &p->second : nullptr;
}

// 阶段 1：每个附着槽独立自然衰减
void ElementReactionSystem::decay_slots(ElementStatus& elem, float dt) {
    for (int i = 0; i < 2; ++i) {
        AttachedSlot& s = elem.attached_slots[i];
        if (!s.is_valid()) continue;
        // 兜底：风/岩绝不能作为附着光环，发现立即清除
        if (!is_attachable_element(s.type)) { s.clear(); continue; }
        s.duration -= dt;
        s.gauge    -= s.decay_rate * dt;
        if (s.gauge <= 0.0f || s.duration <= 0.0f) {
            s.clear();
        }
    }
}

// 阶段 2：感电 DoT 1 秒节拍（水-0.4、雷-0.4，写脉冲反应标签）
void ElementReactionSystem::tick_electrocharged_dot(ElementStatus& elem, float dt) {
    AttachedSlot* h = elem.find_slot(ElementType::Hydro);
    AttachedSlot* e = elem.find_slot(ElementType::Electro);
    if (!h || !e) {
        // 未在共存态 → 冷却清零
        elem.electrocharged_cooldown = 0.0f;
        return;
    }
    if (elem.electrocharged_cooldown <= 0.0f) {
        // 刚进入共存的第一帧 → 启动冷却
        elem.electrocharged_cooldown = 1.0f;
    }
    elem.electrocharged_cooldown -= dt;
    while (elem.electrocharged_cooldown <= 0.0f && h->is_valid() && e->is_valid()) {
        // 一次 DoT 结算：双方各扣 0.4
        h->gauge = std::max(0.0f, h->gauge - 0.4f);
        e->gauge = std::max(0.0f, e->gauge - 0.4f);
        // 写入感电脉冲（位掩码 OR；同帧若同时发生其他新元素反应不再互相覆盖）
        elem.mark_reaction(ReactionType::ElectroCharged);
        // duration 由自然衰减阶段同步处理，此处只以 gauge 为权威
        elem.electrocharged_cooldown += 1.0f;
        if (h->gauge <= 0.0f) h->clear();
        if (e->gauge <= 0.0f) e->clear();
    }
    if (!h || !e) elem.electrocharged_cooldown = 0.0f;
}

// 构建反应顺序优先级队列：水雷共存先和雷反应，余量再和水反应；单元素目标只返回该元素
int ElementReactionSystem::build_priority_slots(ElementStatus& elem, AttachedSlot** ordered_out) {
    int n = 0;
    // 雷元素优先
    if (auto* s = elem.find_slot(ElementType::Electro)) ordered_out[n++] = s;
    for (int i = 0; i < 2; ++i) {
        AttachedSlot& s = elem.attached_slots[i];
        if (!s.is_valid()) continue;
        if (s.type == ElementType::Electro) continue; // 已优先加入
        ordered_out[n++] = &s;
    }
    return n;
}

// update() — 四阶段主流水线
void ElementReactionSystem::update(World& world, float dt) {
    // 规则⑤：对所有带 ElementStatus + ElementICDStatus 的实体生效（玩家/敌人统一处理）
    for (auto e : world.query<ElementStatus, ElementICDStatus>()) {
        auto* elem = world.get_component<ElementStatus>(e);
        auto* icd  = world.get_component<ElementICDStatus>(e);
        if (!elem || !icd) continue;

        // 阶段 0：ICD 计时器每帧衰减（到点强制刷新计数）
        icd_decay_tick(*icd, dt);

        // 阶段 1：每个附着槽独立自然衰减
        decay_slots(*elem, dt);

        // 阶段 2：感电 1 秒共存 DoT 节拍
        tick_electrocharged_dot(*elem, dt);

        // 阶段 3：解析碰撞命中写入的待处理元素
        if (elem->has_pending()) {
            ElementType pending_T = elem->pending_element;
            ElementStrength pending_S = elem->pending_strength;
            float actual_T = get_element_strength_params(pending_S).actual_gauge;

            // 同元素刷新（非共存）：命中相同元素必须刷新已有槽（取更大的量 + 重设持续时间基线）
            // 绝不能落入下方"无规则匹配 → 附着新槽"分支，否则重复的水会写入 slot[1]
            if (AttachedSlot* s = elem->find_slot(pending_T)) {
                ElementStrengthParams const& p = get_element_strength_params(pending_S);
                // 量：取较大者（新的弱命中不稀释原有光环）
                if (p.actual_gauge > s->gauge) {
                    s->gauge         = p.actual_gauge;
                    s->initial_gauge = p.actual_gauge;   // 新基线 = 新附着的量
                }
                // 持续时间：按新强度基线对最终量重新换算，保持 gauge == decay_rate × duration 不变
                float const r = s->gauge / p.actual_gauge;
                s->duration   = p.duration * r;
                s->decay_rate = p.decay_rate;
                elem->clear_pending();
                continue;
            }

            int attached = elem->attached_count();

            if (attached == 0) {
                // 3-A 无元素附着 → 直接附着（若可附着）
                if (is_attachable_element(pending_T)) {
                    attach_element(*elem, pending_T, pending_S);
                }
            } else {
                // 3-C 通用：单元素或共存目标 + 任意触发元素
                AttachedSlot* ordered[2] = {nullptr, nullptr};
                int n = build_priority_slots(*elem, ordered);
                float remaining = actual_T;

                for (int k = 0; k < n && remaining > 0.001f; ++k) {
                    AttachedSlot* s = ordered[k];
                    if (!s || !s->is_valid()) continue;
                    const ReactionRule* rule = find_rule(pending_T, s->type);
                    if (!rule) continue;

                    // 感电分支：共存模式 — 不消耗目标，把待处理元素写入第二槽
                    if (rule->result == ReactionType::ElectroCharged) {
                        if (elem->find_empty_slot()) {
                            attach_element(*elem, pending_T, pending_S);
                        }
                        elem->mark_reaction(rule->result);
                        // 首次建立感电时启动 DoT 计时
                        if (elem->electrocharged_cooldown <= 0.0f)
                            elem->electrocharged_cooldown = 1.0f;
                        remaining = 0.0f;
                        break;
                    }

                    // 普通反应：按比例消耗目标
                    // for 循环天然支持"雷→水优先级顺序连续消耗 remaining"：
                    //   k=0 先和雷反应，remaining 被扣后进入 k=1 继续和水反应，
                    //   最终 remaining 即使仍 >0 也直接丢弃（后手元素绝不能作为残留光环挂上）
                    //   —— 每一次真正执行过消耗的反应都要独立标记脉冲，方便叠加显示多个飘字
                    float cost = rule->target_cost_per_trigger;
                    float max_target_from_trigger = remaining * cost;
                    float consume_target = std::min(s->gauge, max_target_from_trigger);
                    float consume_trigger = (cost > 0.0f) ? consume_target / cost : 0.0f;

                    if (consume_trigger > 0.0f) {
                        if (rule->consumes_target) {
                            s->gauge -= consume_target;
                            if (s->gauge <= 0.0f) s->clear();
                        }
                        remaining -= consume_trigger;
                        elem->mark_reaction(rule->result);
                    }
                }

                // 无规则匹配：已有附着光环 + 待处理元素不反应 → 直接丢弃触发元素
                // 共存只允许水+雷（已在上方感电分支处理）。
                // 例如：草元素遇到不反应的冰光环，绝不能让两者共存。
                // "attached == 0 → 首次附着" 的情况已在 3-A 处理。
            }
            // 消费待处理输入
            elem->clear_pending();
        }

        // 末尾：最终槽清理（量为0或时长为0 → 清空类型）
        for (int i = 0; i < 2; ++i) {
            AttachedSlot& s = elem->attached_slots[i];
            if (s.type != ElementType::None && (s.gauge <= 0.0f || s.duration <= 0.0f)) {
                s.clear();
            }
        }
        // 任一槽位消失 → 感电冷却清零
        if (!elem->find_slot(ElementType::Hydro) || !elem->find_slot(ElementType::Electro)) {
            elem->electrocharged_cooldown = 0.0f;
        }
    }
}
