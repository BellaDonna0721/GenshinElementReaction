#include "OutputSystem.h"
#include "../components/Transform.h"
#include "../components/ElementStatus.h"
#include "../components/ElementPayload.h"
#include "../components/RenderInfo.h"
#include "../components/FacingDirection.h"
#include "../components/Health.h"
#include "../components/IdentityTag.h"
#include "../components/DamageNumberQueue.h"

std::vector<RenderCommand>   g_render_commands;
std::vector<HealthBarInfo>   g_health_bars;
std::vector<DamageNumberInfo> g_damage_numbers;

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

    // 血量比例 → uint8（0~255 线性对应 0%~100%）
    inline uint8_t pack_hp_ratio(float current, float max_hp) {
        if (max_hp <= 0.0001f) return 0;
        float ratio = current / max_hp;
        if (ratio <= 0.0f) return 0;
        if (ratio >= 1.0f) return 255;
        return static_cast<uint8_t>(ratio * 255.0f);
    }

    // 冻元素量条比例 → uint8（gauge / initial_gauge）
    inline uint8_t pack_frozen_ratio(const FrozenState& fz) {
        if (fz.initial_gauge <= 0.0001f) return 0;
        float ratio = fz.gauge / fz.initial_gauge;
        if (ratio <= 0.0f) return 0;
        if (ratio >= 1.0f) return 255;
        return static_cast<uint8_t>(ratio * 255.0f);
    }
}

void OutputSystem::update(World& world, float dt) {
    (void)dt;
    g_render_commands.clear();
    g_health_bars.clear();
    g_damage_numbers.clear();

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
        cmd.reaction_mask_lo = 0;
        cmd.reaction_mask_hi = 0;
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
                // 防止"量条显示异常"问题：当 attached_slots[0] 被清空
                // （例如雷被超载消耗掉）而水在 attached_slots[1] 时，如果不紧凑，
                // 仅剩的那个光环只会出现在外虚线环，内圈为空，视觉上很奇怪。
                // ==============================================================
                const AttachedSlot* valid[2] = {nullptr, nullptr};
                int n_valid = 0;
                for (int i = 0; i < 2 && n_valid < 2; ++i) {
                    const AttachedSlot& s = elem->attached_slots[i];
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

                // 冻结状态：冻元素表现为冰附着，用 bullet_strength 字段传递冻结标志
                // （该字段对敌人恒为 0，复用为 0=正常 1=冻结）
                if (elem->frozen.is_frozen && elem->frozen.gauge > 0.0f) {
                    cmd.bullet_strength = 1;  // 冻结标志
                    uint8_t frozen_gauge = pack_frozen_ratio(elem->frozen);
                    // 冻元素作为内圈主光环（表现为冰），藏元素作为外圈
                    if (n_valid == 0) {
                        // 只有冻元素，无藏元素
                        cmd.element1_type  = static_cast<uint8_t>(ElementType::Cryo);
                        cmd.element1_gauge = frozen_gauge;
                    } else {
                        // 冻元素 + 藏元素：冻元素占内圈，藏元素占外圈
                        // 把原有 valid[0] 移到外圈
                        cmd.element2_type  = cmd.element1_type;
                        cmd.element2_gauge = cmd.element1_gauge;
                        cmd.element1_type  = static_cast<uint8_t>(ElementType::Cryo);
                        cmd.element1_gauge = frozen_gauge;
                    }
                }

                // 消费本帧所有已标记的反应脉冲（uint16 位掩码）
                // 拆成低 8 位 + 高 8 位写入 RenderCommand 两个字节，TS 侧拼成完整掩码
                uint16_t mask = elem->consume_reactions();
                if (mask != 0) {
                    cmd.reaction_mask_lo = static_cast<uint8_t>(mask & 0xFF);
                    cmd.reaction_mask_hi = static_cast<uint8_t>((mask >> 8) & 0xFF);
                }
            }
        }

        // —— 血条：仅敌人显示（玩家血条预留，未来可在 UI 面板单独展示）——
        Health* hp = world.get_component<Health>(e);
        if (hp && ri->entity_type == Identity::Enemy) {
            HealthBarInfo hbi;
            hbi.x     = t->x;
            hbi.y     = t->y - ri->radius - 12.0f;  // 头顶上方
            hbi.width = ri->radius * 2.0f;            // 宽度=敌人直径
            hbi.ratio = pack_hp_ratio(hp->current_hp, hp->max_hp);
            g_health_bars.push_back(hbi);
        }

        auto* face = world.get_component<FacingDirection>(e);
        if (face && ri->entity_type == Identity::Player) {
            cmd.facing_dx = face->dx;
            cmd.facing_dy = face->dy;
        }

        // 子弹特效标记：如果是子弹且带 ElementPayload → 写入 bullet_strength
        // 渲染层据此画外层火焰环（仅 Strong/ExtraStrong 有特效）
        if (ri->entity_type == Identity::Bullet) {
            auto* payload = world.get_component<ElementPayload>(e);
            if (payload) {
                cmd.bullet_strength = static_cast<uint8_t>(payload->strength);
            }
        }

        g_render_commands.push_back(cmd);
    }

    // —— 伤害飘字：从 DamageNumberQueue 转存到全局数组供 TS 读取 ——
    DamageNumberQueue* num_queue = world.get_component<DamageNumberQueue>(0);
    if (num_queue) {
        for (const auto& dn : num_queue->numbers) {
            DamageNumberInfo info;
            info.x      = dn.x;
            info.y      = dn.y;
            info.amount = dn.amount;
            info.r      = dn.r;
            info.g      = dn.g;
            info.b      = dn.b;
            g_damage_numbers.push_back(info);
        }
        // 消费后清空（OutputSystem 是唯一消费者）
        num_queue->numbers.clear();
    }
}
