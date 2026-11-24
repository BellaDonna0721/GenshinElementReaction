#pragma once

// 速度向量组件
//   vx, vy     —— 运动方向（恒为单位向量，由 InputSystem / EnemyAISystem 写入）
//   speed      —— 基础速度大小（像素/秒），spawn 时配置，可被永久增益修改
//   speed_mult —— 临时行为倍率（默认 1.0，如 Flee 逃跑时设为 1.2）
// 最终位移 = (vx, vy) * speed * speed_mult * dt
// 方向与倍率分离：方向永远归一化，倍率只体现在速度侧，避免行为倍率藏在向量长度里
struct Velocity {
    float vx         = 0.0f;  // 方向 X（单位向量）
    float vy         = 0.0f;  // 方向 Y（单位向量）
    float speed      = 0.0f;  // 基础速度（像素/秒）
    float speed_mult = 1.0f;  // 临时行为倍率（默认 1.0）
};
