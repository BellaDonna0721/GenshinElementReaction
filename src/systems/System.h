#pragma once

// =====================================================================
//  System —— 所有 ECS 系统的抽象基类
//
//  职责：定义系统统一的 update(World&, float dt) 接口，由 World 每帧调用。
// =====================================================================

class World;  // 前向声明：update 只接收 World& 引用，不需要完整定义

class System {
public:
    virtual ~System() = default;
    virtual void update(World& world, float dt) = 0;
};
