#pragma once
#include "../core/World.h"
#include "../components/ElementStatus.h"
#include "../importFiles/myUnorderedMap.hpp"
#include <utility>

// Hash for std::pair<ElementType, ElementType>
struct ReactionRuleKeyHash {
    size_t operator()(const std::pair<ElementType, ElementType>& k) const noexcept {
        return (static_cast<size_t>(static_cast<uint8_t>(k.first))  << 8)
             |  static_cast<size_t>(static_cast<uint8_t>(k.second));
    }
};

// 一条元素反应规则：触发元素(子弹) + 目标元素(敌人身上已附着) → 反应结果
struct ReactionRule {
    ElementType  trigger;
    ElementType  target;
    ReactionType result;
    bool         consumes_target;
    bool         consumes_trigger;
    float        target_cost_per_trigger = 1.0f;
};

// 元素反应系统
class ElementReactionSystem : public System {
public:
    ElementReactionSystem();
    void update(World& world, float dt) override;

private:
    //使用改良unordered_map提升缓冲命中率 对比标准库有近40%提升
    myUnorderedMap<
        std::pair<ElementType, ElementType>,
        ReactionRule,
        ReactionRuleKeyHash
    > m_rule_map;

    const ReactionRule* find_rule(ElementType trigger, ElementType target) const;
};