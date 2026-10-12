#include "ElementReactionSystem.h"
#include "../components/ElementICDStatus.h"
#include "../components/DamageQueue.h"
#include <cstdio>
#include <utility>

// 增幅反应倍率表（仅蒸发/融化；其余反应 0 表示当前版本不造成反应伤害）
// 预留：剧变反应（超载/感电/超导/扩散/绽放/燃烧）未来在此加固定值
static float get_amplifying_multiplier(ReactionType rt) {
    switch (rt) {
        case ReactionType::Vaporize:        return 2.0f;  // 水打火
        case ReactionType::ReverseVaporize: return 1.5f;  // 火打水
        case ReactionType::Melt:            return 2.0f;  // 火打冰
        case ReactionType::ReverseMelt:     return 1.5f;  // 冰打火
        default:                            return 0.0f;  // 非增幅反应
    }
}

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
    // 全局伤害队列（挂 Entity 0）：增幅反应触发时写入反应伤害事件
    DamageQueue* dmg_queue = world.get_component<DamageQueue>(0);

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

            // 伤害倍率：默认 1.0（直射伤害），触发增幅反应时覆盖为对应倍率
            // 统一在 clear_pending 前推送一次伤害，保证每次命中只产生一个飘字
            float damage_mult = 1.0f;

            // 同元素刷新（非共存）：命中相同元素必须刷新已有槽，绝不能写入第二槽
            // 规则：量取较大者（补充量 <= 剩余量时不更新）；但更新量不改变主元素的衰减速度，
            //       decay_rate 保持首次附着时的值，仅反算 duration 使 gauge == decay_rate×duration 恒成立。
            if (AttachedSlot* s = elem->find_slot(pending_T)) {
                ElementStrengthParams const& p = get_element_strength_params(pending_S);
                // 量：二者选其多（补充量 > 剩余量才更新）
                if (p.actual_gauge > s->gauge) {
                    s->gauge         = p.actual_gauge;
                    s->initial_gauge = p.actual_gauge;   // UI 基线同步刷新为新量
                }
                // 衰减速度：保持主元素（首次附着）的 decay_rate 不变
                // 仅重算 duration 以维持 gauge == decay_rate × duration 不变量
                if (s->decay_rate > 0.0f) {
                    s->duration = s->gauge / s->decay_rate;
                }
                // 同元素刷新：无反应，damage_mult 保持 1.0
            } else {
                int attached = elem->attached_count();

                if (attached == 0) {
                    // 3-A 无元素附着 → 直接附着（若可附着）
                    if (is_attachable_element(pending_T)) {
                        attach_element(*elem, pending_T, pending_S);
                    }
                    // 首次附着：无反应，damage_mult 保持 1.0
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

                            // 增幅反应：记录倍率（蒸发/融化），最终伤害 = base_damage × mult
                            float mult = get_amplifying_multiplier(rule->result);
                            if (mult > 0.0f) {
                                damage_mult = mult;
                            }
                        }
                    }
                    // 无规则匹配：已有附着光环 + 待处理元素不反应 → 直接丢弃触发元素
                    // 共存只允许水+雷（已在上方感电分支处理）。
                }
            }

            // —— 统一推送一次伤害事件 ——
            //   无反应：amount = base_damage × 1.0
            //   增幅反应：amount = base_damage × mult
            //   保证每次命中只产生一个伤害飘字
            if (dmg_queue && elem->pending_base_damage > 0.0f) {
                DamageEvent rev;
                rev.target   = e;
                rev.amount   = elem->pending_base_damage * damage_mult;
                rev.element  = pending_T;
                rev.reaction = (damage_mult > 1.0f) ? ReactionType::Vaporize : ReactionType::None;
                dmg_queue->events.push_back(rev);
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
