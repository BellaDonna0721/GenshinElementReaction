#include "OutputSystem.h"
#include "../components/Transform.h"
#include "../components/ElementStatus.h"
#include "../components/RenderInfo.h"
#include "../components/FacingDirection.h"

std::vector<RenderCommand> g_render_commands;

namespace {
    // 量 → uint8 量条比例（0~255 = 0% ~ 100%）。
    // 新语义（修复用户可见的"空白80%"问题）：比例 = 当前量 / 初始附着量。
    // 这样弱元素 0.8 初始就是 100%，不再被旧的全局 max=4.0 基线压到 20%。
    // 强元素 1.6、超强 3.2 也都从 100% 开始 — 所有档位归一化。
    inline uint8_t pack_progress_ratio(const AttachedSlot& s) {
        if (s.initial_gauge <= 0.0001f) return 0;
        float ratio = s.gauge / s.initial_gauge;
        if (ratio <= 0.0f) return 0;
        if (ratio >= 1.0f) return 255;
        return static_cast<uint8_t>(ratio * 255.0f);
    }
}

void OutputSystem::update(World& world, float dt) {
    (void)dt;
    g_render_commands.clear();

    for (auto e : world.query<Transform, RenderInfo>()) {
        auto* t  = world.get_component<Transform>(e);
        auto* ri = world.get_component<RenderInfo>(e);

        RenderCommand cmd = {};
        cmd.x            = t->x;
        cmd.y            = t->y;
        cmd.radius       = ri->radius;
        cmd.r            = ri->color_r;
        cmd.g            = ri->color_g;
        cmd.b            = ri->color_b;
        cmd.a            = ri->color_a;
        cmd.entity_type  = static_cast<uint8_t>(ri->entity_type);
        cmd.element1_type  = 0;
        cmd.element2_type  = 0;
        cmd.element1_gauge = 0;
        cmd.element2_gauge = 0;
        cmd.reaction_type  = 0;
        cmd.facing_dx = 0.0f;
        cmd.facing_dy = 0.0f;

        auto* elem = world.get_component<ElementStatus>(e);
        if (elem) {
            if (ri->entity_type == Identity::Enemy) {
                // ==============================================================
                // 写入光环字段前先紧凑有效附着槽。
                // 元素1（内实心环）= 第一个有效光环（主显示）
                // 元素2（外虚线环）= 第二个有效光环（仅共存时存在）
                //
                // 防止"量条显示异常"问题：当 slots[0] 被清空
                // （例如雷被超载消耗掉）而水在 slots[1] 时，如果不紧凑，
                // 仅剩的那个光环只会出现在外虚线环，内圈为空，视觉上很奇怪。
                // ==============================================================
                const AttachedSlot* valid[2] = {nullptr, nullptr};
                int n_valid = 0;
                for (int i = 0; i < 2 && n_valid < 2; ++i) {
                    const AttachedSlot& s = elem->slots[i];
                    if (!s.is_valid()) continue;
                    if (!is_attachable_element(s.type)) continue;
                    valid[n_valid++] = &s;
                }
                if (n_valid >= 1) {
                    cmd.element1_type  = static_cast<uint8_t>(valid[0]->type);
                    cmd.element1_gauge = pack_progress_ratio(*valid[0]);
                }
                if (n_valid >= 2) {
                    cmd.element2_type  = static_cast<uint8_t>(valid[1]->type);
                    cmd.element2_gauge = pack_progress_ratio(*valid[1]);
                }

                // 消费一次性 pending_reaction 脉冲
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
