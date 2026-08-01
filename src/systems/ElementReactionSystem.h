#pragma once
#include "../core/World.h"
#include "../components/ElementStatus.h"

// 一条元素反应规则：触发元素(子弹) + 目标元素(敌人身上已附着) → 反应结果
struct ReactionRule {
    ElementType  trigger;              // 触发元素（子弹携带的元素）
    ElementType  target;               // 目标元素（敌人身上当前附着的元素）
    ReactionType result;               // 反应结果（None=不反应）
    bool         consumes_target;      // 反应后是否【允许】清除目标元素（精细计算时只作为门槛，实际是否清光看 gauge 比值）
    bool         consumes_trigger;     // 反应后是否不把触发元素再附着上去（后手元素不残留时恒 true）
    float        target_cost_per_trigger = 1.0f;  // 每 1 单位【触发元素实际量】可消耗多少【目标元素实际量】
                                                  //  克制元素（如水打、火打冰）= 2.0 → 1 单位触发量顶 2 单位目标量
                                                  //  被克制元素（如火打水、冰打火）= 0.5 → 1 单位触发量只顶 0.5 单位目标量
                                                  //  默认 1.0 → 1:1 对等消耗（不破坏未来新增其他 1:1 反应）
};

// 元素反应系统：第 5 步流水线（碰撞之后）
//   1. 已附着元素 gauge/duration 随时间线性衰减
//   2. 有 pending_element → 查表匹配 ReactionRule
//      → 命中则写 pending_reaction 一次性脉冲（TS 侧拿到后自行启动渐隐计时）
//      → 根据 consumes_* 决定是否覆盖附着新元素
//      → 未命中反应 → 直接覆盖附着新元素
class ElementReactionSystem : public System {
public:
    ElementReactionSystem();                   //构造函数里注册 ReactionRule（目前只注册了蒸发+逆蒸发2条）
    void update(World& world, float dt) override;

private:
    ReactionRule m_rules[28];                  //规则表
    int m_rule_count = 0;                      //实际已注册规则数

    //查表：给定 触发元素+目标元素 → 返回匹配的 ReactionRule*（找不到返回 nullptr）
    const ReactionRule* find_rule(ElementType trigger, ElementType target) const;
};
