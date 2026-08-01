#include "OutputSystem.h"
#include "../components/Transform.h"
#include "../components/ElementStatus.h"
#include "../components/RenderInfo.h"
#include "../components/FacingDirection.h"

std::vector<RenderCommand> g_render_commands;

void OutputSystem::update(World& world, float dt) {
    (void)dt;
    g_render_commands.clear();

    for (auto e : world.query<Transform, RenderInfo>()) {
        auto* t  = world.get_component<Transform>(e);
        auto* ri = world.get_component<RenderInfo>(e);

        RenderCommand cmd = {};
        cmd.x           = t->x;
        cmd.y           = t->y;
        cmd.radius      = ri->radius;
        cmd.r           = ri->color_r;
        cmd.g           = ri->color_g;
        cmd.b           = ri->color_b;
        cmd.a           = ri->color_a;
        cmd.entity_type = static_cast<uint8_t>(ri->entity_type);
        cmd.element_type = 0;
        cmd.element_gauge = 0.0f;
        cmd.reaction_type = 0;
        cmd.facing_dx = 0.0f;
        cmd.facing_dy = 0.0f;

        auto* elem = world.get_component<ElementStatus>(e);
        if (elem) {
            if (elem->is_active() && ri->entity_type == Identity::Enemy) {
                // 双重保险：Anemo/Geo/Dendro 是"后手触发元素"，不允许作为附着元素渲染光环/量条
                if (is_attachable_element(elem->type)) {
                    cmd.element_type   = static_cast<uint8_t>(elem->type);
                    cmd.element_gauge  = elem->gauge;
                }
            }
            if (ri->entity_type == Identity::Enemy) {
                // 消费一次性 pending_reaction 脉冲：
                //   命中反应的那一帧非 None → 本帧传给 TS reaction_type>0
                //   consume_reaction() 读完就清 None → 下一帧 reaction_type 自动回到 0
                //   显示时长完全由 TS 侧 render.ts 的 cached.t=1.5s 渐隐负责
                ReactionType rt = elem->consume_reaction();
                if (rt != ReactionType::None) {
                    cmd.reaction_type = static_cast<uint8_t>(rt);
                }
            }
        }

        auto* face = world.get_component<FacingDirection>(e);
        if (face && ri->entity_type == Identity::Player) {
            cmd.facing_dx = face->dx;
            cmd.facing_dy = face->dy;
        }

        g_render_commands.push_back(cmd);
    }
}