#pragma once
#include <cstdint>
#include <vector>

// =====================================================================
//  HitFxQueue —— 命中特效事件队列（单例组件，挂 Entity 0）
//
//  写入方：OutputSystem（帧首清空，消费 HitEventQueue 生成命中/撞墙特效）
//  消费方：wasm_api 直接读取 items vector 供 TS 渲染
//
//  生命周期由写入方管理：OutputSystem::update 开头 clear，本帧写入的
//  特效留存至下一帧 OutputSystem 清空之前，TS 在系统管线结束后读取。
//
//  特效内容（TS 侧负责逐帧动画）：
//    1. 闪光：命中点元素色径向渐变，快速放大淡出（0.15s）
//    2. 光环：元素色圆环从中心向外扩散，alpha 渐隐（0.3s）
//    3. 粒子：8~12 颗小粒子向四周发散，减速+淡出（0.4s）
// =====================================================================

// 单例组件：本帧产生的所有命中特效
struct HitFxQueue {
    // 子结构：单条命中特效事件（内聚在 HitFxQueue 中，非独立组件）
    struct HitFx {
        float   x, y;       // 命中点坐标
        uint8_t r, g, b;    // 元素主色（决定闪光/光环/粒子颜色）
        uint8_t hit_type;   // 0=命中敌人  1=命中墙体
        float   scale;      // 尺寸系数（按敌人半径自适应，墙体固定 1.0）
    };

    std::vector<HitFx> items;
};
