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
    uint8_t reaction_type;  //[18]     本帧触发的反应 ReactionType（脉冲，0=None）
    uint8_t element2_type;  //[19]     附着元素2（外圈，共存专用，ElementType，0=None）
    uint8_t element1_gauge; //[20]     元素1剩余量归一化：0-255 线性对应 0-4.0f actual_gauge
    uint8_t element2_gauge; //[21]     元素2剩余量归一化（同上）
    uint8_t _pad2;          //[22]
    uint8_t _pad3;          //[23]
    float   facing_dx;      //[24,28)  玩家朝向X
    float   facing_dy;      //[28,32)  玩家朝向Y
};

extern std::vector<RenderCommand> g_render_commands;

class OutputSystem : public System {
public:
    void update(World& world, float dt) override;
};
