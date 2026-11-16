#pragma once
#include "../core/World.h"
#include "../components/singletonComponents/DamageNumberQueue.h"
#include "../components/singletonComponents/HitFxQueue.h"
#include <cstdint>
#include <vector>

// 渲染命令：C++ → WASM线性内存 → TypeScript Canvas 之间的 36 字节精确协议
// 任意一边字段顺序/大小改动，另一边必须同步！
struct RenderCommand {
    float   x, y;           //[0,8)    圆心坐标（像素）
    float   radius;         //[8,12)   渲染半径（像素）
    uint8_t r, g, b, a;     //[12,16)  RGBA 颜色
    uint8_t entity_type;    //[16]     实体类型 Identity 枚举值(0=Player/1=Enemy/2=Bullet)
    uint8_t element1_type;  //[17]     附着元素1（内圈，ElementType，0=None）
    uint8_t reaction_mask_lo;  //[18]  本帧触发反应位掩码（低 8 位，index = static_cast<int>(ReactionType)）
    uint8_t element2_type;  //[19]     附着元素2（外圈，共存专用，ElementType，0=None）
    uint8_t element1_gauge; //[20]     元素1剩余量归一化：0-255 线性对应 0-100% 进度
    uint8_t element2_gauge; //[21]     元素2剩余量归一化（同上）
    uint8_t bullet_strength;//[22]     子弹强度档（ElementStrength：0Weak/1Medium/2Strong/3Extra；敌人玩家填0）
    uint8_t reaction_mask_hi;  //[23]  反应位掩码高 8 位（与 [18] 拼成 uint16）
    float   facing_dx;      //[24,28)  玩家朝向X / 子弹速度方向X
    float   facing_dy;      //[28,32)  玩家朝向Y / 子弹速度方向Y
    uint8_t enemy_type;     //[32]     敌人本体元素类型（ElementType 值，非敌人填 0）
    uint8_t _pad[3];        //[33,36)  显式补齐到 36 字节（保持 sizeof 确定，不依赖编译器隐式 padding）
};
static_assert(sizeof(RenderCommand) == 36, "RenderCommand 必须保持 36 字节");

extern std::vector<RenderCommand> g_render_commands;

// =====================================================================
//  血条数据（独立于 36 字节 RenderCommand，走单独通道）
//  OutputSystem 填充，TS 侧读取并在实体头顶绘制血条
// =====================================================================
struct HealthBarInfo {
    float   x, y;       // 血条中心坐标（TS 侧据此定位，通常在实体头顶）
    float   width;      // 血条总宽度（像素）
    uint8_t ratio;      // 当前血量比例 0~255 线性对应 0%~100%
    uint8_t enemy_type; // 敌人类型（ElementType 值，临时用于血条显示类型名）
};

extern std::vector<HealthBarInfo> g_health_bars;

// =====================================================================
//  伤害飘字数据（独立通道）
//  DamageSystem 写入 DamageNumberQueue（单例组件），wasm_api 直接读取组件
//  中的 vector 供 TS 渲染。TS 侧负责渐隐 + 上浮动画。
//  结构体 DamageNumber 定义见 DamageNumberQueue.h。
// =====================================================================

// =====================================================================
//  墙体渲染数据（独立通道，走矩形而非圆形，不占用 36 字节 RenderCommand）
//  OutputSystem 收集 Identity=Wall 的实体写入，TS 侧绘制填充矩形+描边
//  字节布局（20 字节）：
//    [0,4)   x       (f32)  矩形中心 X
//    [4,8)   y       (f32)  矩形中心 Y
//    [8,12)  width   (f32)  矩形宽
//    [12,16) height  (f32)  矩形高
//    [16]    r       (u8)
//    [17]    g       (u8)
//    [18]    b       (u8)
//    [19]    a       (u8)
// =====================================================================
struct WallRenderInfo {
    float   x, y;
    float   width, height;
    uint8_t r, g, b, a;
};

extern std::vector<WallRenderInfo> g_walls;

// =====================================================================
//  命中特效数据（独立通道）
//  CollisionSystem 写入 HitFxQueue（单例组件），wasm_api 直接读取组件中的
//  vector 供 TS 渲染。TS 侧负责三层动画（闪光 + 扩散光环 + 迸溅粒子）。
//
//  字节布局（16 字节）：
//    [0,4)   x          (f32)  命中点 X
//    [4,8)   y          (f32)  命中点 Y
//    [8]     r          (u8)
//    [9]     g          (u8)
//    [10]    b          (u8)
//    [11]    hit_type   (u8)   0=命中敌人  1=命中墙体
//    [12,16) scale      (f32)  尺寸系数（敌人按半径自适应）
//  结构体定义见 HitFxQueue.h（HitFx）。
// =====================================================================

class OutputSystem : public System {
public:
    void update(World& world, float dt) override;
};
