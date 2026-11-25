#pragma once
#include <vector>
#include <cstdint>
#include "../../core/Entity.h"

// =====================================================================
//  HitEventQueue —— 碰撞事件队列（单例组件，挂 Entity 0）
//
//  定位：碰撞事实的唯一权威出口。CollisionSystem 是唯一做几何检测的系统，
//        检测到碰撞后 push 一条事件，不再直接处理伤害/附着/特效/归池。
//        其他系统一律消费本队列，禁止各自重做几何判定。
//
//  写入方：CollisionSystem
//  消费方（按流水线顺序）：
//    1. PickupSystem —— 读 PlayerTouchPickup，执行道具效果并销毁道具
//    2. ElementReactionSystem —— 读 BulletHitEnemy 写 ElementStatus.pending（含 ICD）
//    3. DamageSystem —— 读 EnemyTouchPlayer，冷却就绪则写 DamageQueue（接触伤害）
//    4. OutputSystem —— 读 BulletHit* 生成命中/撞墙特效、归池子弹；最后清空队列
//
//  生命周期：OutputSystem 帧末消费后 clear，CollisionSystem 下帧重新写入。
// =====================================================================

struct HitEventQueue {
    enum class Type : uint8_t {
        BulletHitEnemy,   // 子弹命中敌人：aggressor=子弹, victim=敌人
        BulletHitWall,    // 子弹撞墙：  aggressor=子弹, victim=INVALID_ENTITY
        EnemyTouchPlayer, // 敌人接触玩家：aggressor=敌人, victim=玩家
        PlayerTouchPickup,// 玩家接触道具：aggressor=玩家, victim=道具
    };

    // 单条碰撞事件
    struct HitEvent {
        Type   type;
        Entity aggressor;  // 发起方实体（子弹 / 敌人 / 玩家）
        Entity victim;     // 承受方实体（敌人 / 玩家 / 道具；撞墙为 INVALID_ENTITY）
        float  x, y;       // 接触点坐标
    };

    std::vector<HitEvent> events;
};
