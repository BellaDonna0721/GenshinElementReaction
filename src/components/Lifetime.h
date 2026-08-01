#pragma once

// 通用生命周期组件：任何临时实体（子弹、AoE、特效等）都可以挂它
// 由 LifetimeSystem 每帧扣减 remaining，到 0 就回收/销毁
struct Lifetime {
    float remaining = 0.0f;  //剩余存活时间（秒），<= 0 时销毁
};
