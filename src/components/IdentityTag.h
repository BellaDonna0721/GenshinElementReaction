#pragma once

// 实体身份类型枚举（逻辑层内部使用，不暴露给 TS/渲染层）
// 与 RenderInfo.entity_type 语义一致但物理分离：逻辑层只读取 IdentityTag
// RenderInfo.entity_type 完全交给渲染系统 OutputSystem 使用
enum class Identity : uint8_t {
    Player   = 0,  //玩家
    Enemy    = 1,  //敌人
    Bullet   = 2,  //子弹/投射物
    AoEZone  = 3,  //AoE 区域（预留，RenderCommand 协议兼容值）
};

// 统一身份标签组件：用一个组件替代原来的 BulletTag/PlayerTag/EnemyTag 三个空标签
// 每个"有身份的"实体（玩家/敌人/子弹）都持有一份 IdentityTag
// 身份在 spawn / fire 时一次性写入，运行时不修改
struct IdentityTag {
    Identity identity_type = Identity::Player;  //实体身份（Player / Enemy / Bullet）
};
