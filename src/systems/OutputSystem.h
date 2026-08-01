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
    uint8_t entity_type;    //[16]     实体类型 Identity 枚举值(0=Player/1=Enemy/2=Bullet)（复用 IdentityTag 的枚举，避免重复定义）
    uint8_t element_type;   //[17]     附着元素 ElementType(仅敌人有效，None=0)
    uint8_t reaction_type;  //[18]     本帧触发的反应 ReactionType（一次性脉冲：命中帧非0，下一帧立即清0，TS 侧自行管渐隐时长）
    uint8_t _pad1;          //[19]     显式填充，锁住 32 字节总大小对齐
    float   element_gauge;  //[20,24)  附着元素剩余量(画元素环宽度)
    float   facing_dx;      //[24,28)  玩家朝向X(画白色小三角)
    float   facing_dy;      //[28,32)  玩家朝向Y(画白色小三角)
};

extern std::vector<RenderCommand> g_render_commands;  //全局渲染命令数组（OutputSystem 写，wasm_api 给 TS 裸指针）

// 输出系统：第 6 步流水线（最后一个跑）
//   先 clear() → 遍历 Transform+RenderInfo 实体 → 填附加信息(元素/反应/朝向)
//   → push_back 进 g_render_commands → TS 侧每帧读一次画出来
class OutputSystem : public System {
public:
    void update(World& world, float dt) override;
};
