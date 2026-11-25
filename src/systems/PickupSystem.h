#pragma once
#include "../systems/System.h"

// =====================================================================
//  PickupSystem —— 道具拾取系统
//
//  职责：
//    · 消费 CollisionSystem 写入的 PlayerTouchPickup 碰撞事件
//    · 拾取后根据道具挂载的效果组件执行对应效果：
//        ShieldPickup → GameState.shield_count += layers（受 shield_max 约束）
//    · 拾取后销毁道具实体
//
//  扩展方式：新增道具类型只需定义新效果组件 + 在此系统加一个分支。
//  流水线位置：CollisionSystem 之后、ElementReactionSystem 之前。
// =====================================================================

class PickupSystem : public System {
public:
    void update(World& world, float dt) override;
};
