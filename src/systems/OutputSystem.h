#pragma once
#include "../core/World.h"
#include <cstdint>
#include <vector>

// 渲染命令：C++ → WASM线性内存 → TypeScript Canvas 之间的 32 字节精确协议
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
    float   facing_dx;      //[24,28)  玩家朝向X
    float   facing_dy;      //[28,32)  玩家朝向Y
};

extern std::vector<RenderCommand> g_render_commands;

// =====================================================================
//  血条数据（独立于 32 字节 RenderCommand，走单独通道）
//  OutputSystem 填充，TS 侧读取并在实体头顶绘制血条
// =====================================================================
struct HealthBarInfo {
    float   x, y;       // 血条中心坐标（TS 侧据此定位，通常在实体头顶）
    float   width;      // 血条总宽度（像素）
    uint8_t ratio;      // 当前血量比例 0~255 线性对应 0%~100%
};

extern std::vector<HealthBarInfo> g_health_bars;

// =====================================================================
//  伤害飘字数据（独立通道）
//  DamageSystem 写入 DamageNumberQueue，OutputSystem 转存到全局数组供 TS 读取
//  TS 侧负责渐隐 + 上浮动画（复用反应飘字的客户端缓存模式）
// =====================================================================
struct DamageNumberInfo {
    float   x, y;       // 出现位置（怪物坐标）
    int     amount;     // 整数伤害
    uint8_t r, g, b;    // 元素颜色
};

extern std::vector<DamageNumberInfo> g_damage_numbers;

class OutputSystem : public System {
public:
    void update(World& world, float dt) override;
};
