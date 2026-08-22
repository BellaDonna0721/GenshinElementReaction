// ========================================================================
// ICD（元素附着冷却）单元测试（修复版：7 元素 × 2 档 = 14 组独立）
// 编译运行：g++ -std=c++17 -I../src benchmark/icd_test.cpp -o benchmark/icd_test.exe
// ========================================================================
#include "../src/components/ElementStatus.h"
#include <cstdio>

static int s_pass = 0;
static int s_fail = 0;

#define EXPECT(cond, name, fmt, ...) do {                                    \
    if (cond) {                                                              \
        std::printf("  [PASS] %s\n", name);                                  \
        ++s_pass;                                                            \
    } else {                                                                 \
        std::printf("  [FAIL] %s : " fmt "\n", name, ##__VA_ARGS__);         \
        ++s_fail;                                                            \
    }                                                                        \
} while (0)

// 取特定 (元素×档位) CounterSlot 的只读引用做断言
static inline const ElementICDStatus::CounterSlot& peek(const ElementICDStatus& icd, ElementType t, ElementStrength s) {
    int ei = static_cast<int>(t);
    int gi = (s == ElementStrength::Strong) ? 1 : 0;
    return icd.icd_counters[ei][gi];
}

// ===========================================================================
// 用例 1：单 Weak(Pyro) 档命中序列 —— 放行位严格 1,4,7,10,13,16,19
// ===========================================================================
static void test_case1_weak_pyro_sequence() {
    std::printf("\n===== Case 1：Weak(Pyro) 序列 1,4,7,10,13,16,19 放行 =====\n");
    ElementICDStatus icd;
    const ElementType T = ElementType::Pyro;
    const ElementStrength S = ElementStrength::Weak;

    bool expected[20] = {};
    expected[0]  = true;  // #1
    expected[3]  = true;  // #4
    expected[6]  = true;  // #7
    expected[9]  = true;  // #10
    expected[12] = true;  // #13
    expected[15] = true;  // #16
    expected[18] = true;  // #19

    for (int i = 0; i < 20; ++i) {
        bool allow = icd_hit_allowed(icd, T, S);
        const auto& slot = peek(icd, T, S);
        char label[80];
        std::snprintf(label, sizeof(label),
            "Weak(Pyro) 命中 #%d: counter=%d timer=%.2f allow=%d (期望 %d)",
            i + 1, slot.counter, slot.timer, (int)allow, (int)expected[i]);
        EXPECT(allow == expected[i], label, "");
    }
    // 其他 15 个 slot（共 16 行，减去 Weak(Pyro) 自己 1 行）必须完全干净
    const int SELF_EI = static_cast<int>(ElementType::Pyro);  // 1
    const int SELF_GI = 0;                                     // Weak
    for (int ei = 0; ei < ElementICDStatus::ELEM_DIM; ++ei) {
        ElementType elem = static_cast<ElementType>(ei);
        for (int gi = 0; gi < ElementICDStatus::GEAR_DIM; ++gi) {
            if (ei == SELF_EI && gi == SELF_GI) continue;
            ElementStrength g = (gi == 0) ? ElementStrength::Weak : ElementStrength::Strong;
            const auto& s = peek(icd, elem, g);
            char label[96];
            std::snprintf(label, sizeof(label),
                "交叉污染检查：其余 %d 组必须干净 — ei=%d(elem=%d) gi=%d  counter=%d timer=%.2f == 0/0",
                ElementICDStatus::ELEM_DIM*ElementICDStatus::GEAR_DIM - 1,
                ei, (int)elem, gi, s.counter, s.timer);
            EXPECT(s.counter == 0 && s.timer == 0.0f, label, "");
        }
    }
}

// ===========================================================================
// 用例 2：timer 到点强制刷新（2.5s 到点 → 该槽 counter 清零，其他 13 槽不变）
// ===========================================================================
static void test_case2_timer_expiry_refresh() {
    std::printf("\n===== Case 2：2.5s 到点 → 该槽清零；其他 13 槽保持独立 =====\n");
    ElementICDStatus icd;

    // Strong(Electro) 打 4 发（序列 ✅❌❌✅）
    ElementType T = ElementType::Electro;
    ElementStrength S = ElementStrength::Strong;
    bool r1 = icd_hit_allowed(icd, T, S);
    bool r2 = icd_hit_allowed(icd, T, S);
    bool r3 = icd_hit_allowed(icd, T, S);
    bool r4 = icd_hit_allowed(icd, T, S);
    EXPECT(r1 && !r2 && !r3 && r4,
           "Strong(Electro) 前 4 发 = ✅❌❌✅",
           "r1=%d r2=%d r3=%d r4=%d", (int)r1,(int)r2,(int)r3,(int)r4);
    EXPECT(peek(icd, T, S).counter == 4,
           "Strong(Electro) counter==4",
           "counter=%d", peek(icd, T, S).counter);

    // 让时间流过 3.0s（>2.5s 到点，14 槽独立衰减）
    icd_decay_tick(icd, 3.0f);

    // 该 Electro Strong 槽必须清零
    EXPECT(peek(icd, T, S).timer == 0.0f && peek(icd, T, S).counter == 0,
           "Strong(Electro) 到点 → timer=0/counter=0（独立清零）",
           "timer=%.2f counter=%d",
           peek(icd, T, S).timer, peek(icd, T, S).counter);

    // 同时 Weak(Hydro) 槽必须仍干净（没被 Electro 的衰减污染）
    EXPECT(peek(icd, ElementType::Hydro, ElementStrength::Weak).timer == 0.0f,
           "到点衰减不影响 Weak(Hydro)=0（其他槽独立）",
           "timer=%.2f", peek(icd, ElementType::Hydro, ElementStrength::Weak).timer);

    // 下一发 Strong(Electro) = 新一轮重启 #1 ✅
    bool r5 = icd_hit_allowed(icd, T, S);
    EXPECT(r5 == true, "到点后下一发 Strong(Electro)=新一轮 #1 → ✅",
           "r5=%d counter=%d timer=%.2f",
           (int)r5, peek(icd, T, S).counter, peek(icd, T, S).timer);
}

// ===========================================================================
// 用例 3：档位独立（Weak vs Strong，同一元素也不共享）
// ===========================================================================
static void test_case3_gear_independent_same_element() {
    std::printf("\n===== Case 3：同元素 × Weak/Strong 档位独立 =====\n");
    ElementICDStatus icd;
    ElementType T = ElementType::Cryo;

    // 穿插：Cryo-Weak / Cryo-Strong 交替
    bool w1 = icd_hit_allowed(icd, T, ElementStrength::Weak);     // ✅ c=1
    bool s1 = icd_hit_allowed(icd, T, ElementStrength::Strong);   // ✅ c=1
    bool w2 = icd_hit_allowed(icd, T, ElementStrength::Weak);     // ❌ c=2
    bool s2 = icd_hit_allowed(icd, T, ElementStrength::Strong);   // ❌ c=2
    bool w3 = icd_hit_allowed(icd, T, ElementStrength::Weak);     // ❌ c=3
    bool s3 = icd_hit_allowed(icd, T, ElementStrength::Strong);   // ❌ c=3
    bool w4 = icd_hit_allowed(icd, T, ElementStrength::Weak);     // ✅ c=4
    bool s4 = icd_hit_allowed(icd, T, ElementStrength::Strong);   // ✅ c=4

    EXPECT(w1 && !w2 && !w3 && w4, "Cryo Weak 序列 ✅❌❌✅",
           "w1=%d w2=%d w3=%d w4=%d", (int)w1,(int)w2,(int)w3,(int)w4);
    EXPECT(s1 && !s2 && !s3 && s4, "Cryo Strong 序列 ✅❌❌✅（不与 Weak 共享）",
           "s1=%d s2=%d s3=%d s4=%d", (int)s1,(int)s2,(int)s3,(int)s4);
    EXPECT(peek(icd, T, ElementStrength::Weak).counter == 4,
           "Cryo Weak counter==4", "counter=%d",
           peek(icd, T, ElementStrength::Weak).counter);
    EXPECT(peek(icd, T, ElementStrength::Strong).counter == 4,
           "Cryo Strong counter==4", "counter=%d",
           peek(icd, T, ElementStrength::Strong).counter);
}

// ===========================================================================
// 用例 4：每敌独立（两个敌人 A/B 的 ICD 完全不共享，之前的老规则）
// ===========================================================================
static void test_case4_per_enemy_independent() {
    std::printf("\n===== Case 4：敌人 A/B ICD 互不污染 =====\n");
    ElementICDStatus A;
    ElementICDStatus B;
    ElementType T = ElementType::Dendro;
    ElementStrength S = ElementStrength::Weak;

    // 先打 A：Weak(Dendro) 3 发 → ✅❌❌
    bool A1 = icd_hit_allowed(A, T, S);
    bool A2 = icd_hit_allowed(A, T, S);
    bool A3 = icd_hit_allowed(A, T, S);
    EXPECT(A1 && !A2 && !A3, "A：#1 ✅ #2 ❌ #3 ❌",
           "A1=%d A2=%d A3=%d", (int)A1,(int)A2,(int)A3);
    EXPECT(peek(A, T, S).counter == 3, "A counter=3",
           "counter=%d", peek(A, T, S).counter);

    // B 必须完全干净
    EXPECT(peek(B, T, S).counter == 0 && peek(B, T, S).timer == 0.0f,
           "打 A 期间 B 从未被打 → B 干净 0/0",
           "counter=%d timer=%.2f", peek(B, T, S).counter, peek(B, T, S).timer);

    // B 的第 1 发 → ✅ 放行
    bool B1 = icd_hit_allowed(B, T, S);
    EXPECT(B1 == true, "B #1 → ✅（独立新序列）",
           "B1=%d B_counter=%d", (int)B1, peek(B, T, S).counter);
    EXPECT(peek(A, T, S).counter == 3, "A 仍保持 counter=3（B 的写入不回写 A）",
           "A counter=%d", peek(A, T, S).counter);
}

// ===========================================================================
// 用例 5：ICD 拦截时反应也被拦截（老规则① B 选项；集成链路）
// ===========================================================================
static void test_case5_icd_blocks_reaction_too() {
    std::printf("\n===== Case 5：被 ICD 拦截 → 不挂元素 + 不触发反应 =====\n");
    ElementStatus enemy;
    attach_element(enemy, ElementType::Hydro, ElementStrength::Weak);
    EXPECT(enemy.attached_slots[0].type == ElementType::Hydro && enemy.attached_slots[0].gauge > 0.5f,
           "初始：敌人 Hydro gauge≈0.8",
           "type=%d gauge=%.2f", (int)enemy.attached_slots[0].type, enemy.attached_slots[0].gauge);

    ElementICDStatus icd;
    ElementType Pyro = ElementType::Pyro;
    ElementStrength W = ElementStrength::Weak;

    auto try_process_pending_mini = [](ElementStatus& e) {
        if (!e.has_pending()) return;
        ElementType T = e.pending_element;
        ElementStrength S = e.pending_strength;
        float g = get_element_strength_params(S).actual_gauge;
        for (int i = 0; i < 2; ++i) {
            if (!e.attached_slots[i].is_valid()) continue;
            if (T == ElementType::Pyro && e.attached_slots[i].type == ElementType::Hydro) {
                float cost = std::min(e.attached_slots[i].gauge, g * 0.5f);
                e.attached_slots[i].gauge = std::max(0.0f, e.attached_slots[i].gauge - cost);
                if (e.attached_slots[i].gauge <= 0.0f) e.attached_slots[i].clear();
                e.pending_reaction = ReactionType::ReverseVaporize;
                break;
            }
        }
        e.clear_pending();
    };

    // #1（Pyro Weak 放行）
    if (icd_hit_allowed(icd, Pyro, W)) { enemy.pending_element = Pyro; enemy.pending_strength = W; }
    try_process_pending_mini(enemy);
    EXPECT(enemy.pending_reaction == ReactionType::ReverseVaporize,
           "#1(✅) Pyro × Hydro → ReverseVaporize 脉冲",
           "reaction=%d", (int)enemy.pending_reaction);
    enemy.consume_reaction();

    // #2（Pyro Weak 拦截 → pending 不写 → reaction=None）
    bool a2 = icd_hit_allowed(icd, Pyro, W);
    EXPECT(a2 == false, "#2 被 ICD 拦截",
           "a2=%d counter=%d", (int)a2, peek(icd, Pyro, W).counter);
    if (a2) { enemy.pending_element = Pyro; enemy.pending_strength = W; }
    try_process_pending_mini(enemy);
    EXPECT(enemy.pending_reaction == ReactionType::None,
           "#2(❌) → reaction=None（反应也被拦截）",
           "reaction=%d", (int)enemy.pending_reaction);
    float gauge_after_1 = enemy.attached_slots[0].type == ElementType::Hydro ? enemy.attached_slots[0].gauge : 0.0f;
    EXPECT(gauge_after_1 > 0.3f && gauge_after_1 < 0.5f,
           "Hydro 保持 ≈0.4（没有再次被消耗，证明反应没发生）",
           "gauge=%.2f", gauge_after_1);

    // #3 拦截；#4 放行再次触发反应
    icd_hit_allowed(icd, Pyro, W);                // #3 ❌
    bool a4 = icd_hit_allowed(icd, Pyro, W);      // #4 ✅
    EXPECT(a4 == true, "#4(✅) 放行",
           "a4=%d counter=%d", (int)a4, peek(icd, Pyro, W).counter);
    if (a4) { enemy.pending_element = Pyro; enemy.pending_strength = W; }
    try_process_pending_mini(enemy);
    EXPECT(enemy.pending_reaction == ReactionType::ReverseVaporize,
           "#4(✅) → 重新触发 ReverseVaporize",
           "reaction=%d", (int)enemy.pending_reaction);
}

// ===========================================================================
// 用例 6（新增 · 本次修复核心验证）：不同元素之间 ICD 独立
//   A 敌人 Pyro 连续命中 2 次（第 1✅ 第 2❌，counter=2）后，
//   立刻切 Hydro 打第 1 发 → Hydro 的 counter=1，应放行 ✅
//   同时 Pyro 序列的 counter 必须保持 2 不变（没被 Hydro 的命中改动）
// ===========================================================================
static void test_case6_cross_element_independent() {
    std::printf("\n===== Case 6【本次修复核心】：不同元素之间 ICD 完全独立 =====\n");
    ElementICDStatus icd;

    // —— 阶段 1：Pyro(Weak) 打 2 发 ——
    bool P1 = icd_hit_allowed(icd, ElementType::Pyro, ElementStrength::Weak);
    bool P2 = icd_hit_allowed(icd, ElementType::Pyro, ElementStrength::Weak);
    EXPECT(P1 == true  && P2 == false,
           "阶段 1：Pyro(Weak) #1 ✅  #2 ❌",
           "P1=%d P2=%d", (int)P1, (int)P2);
    int pyro_c2 = peek(icd, ElementType::Pyro, ElementStrength::Weak).counter;
    EXPECT(pyro_c2 == 2, "Pyro(Weak) counter==2（在被 Hydro 命中前）",
           "counter=%d", pyro_c2);

    // —— 阶段 2：切 Hydro(Weak) 打第 1 发 —— 必须算 Hydro 的新序列 #1
    bool H1 = icd_hit_allowed(icd, ElementType::Hydro, ElementStrength::Weak);
    EXPECT(H1 == true,
           "阶段 2：Hydro(Weak) 的第 1 发必须 #1 ✅ 放行（不能延续 Pyro 的 counter=2）",
           "H1=%d Hydro_counter=%d Hydro_timer=%.2f",
           (int)H1, peek(icd, ElementType::Hydro, ElementStrength::Weak).counter,
           peek(icd, ElementType::Hydro, ElementStrength::Weak).timer);
    EXPECT(peek(icd, ElementType::Hydro, ElementStrength::Weak).counter == 1,
           "Hydro(Weak) counter==1",
           "counter=%d", peek(icd, ElementType::Hydro, ElementStrength::Weak).counter);

    // —— 关键断言：Hydro 的命中绝不能改动 Pyro 的计数/timer ——
    int pyro_c_now = peek(icd, ElementType::Pyro, ElementStrength::Weak).counter;
    float pyro_t_now = peek(icd, ElementType::Pyro, ElementStrength::Weak).timer;
    EXPECT(pyro_c_now == 2 && pyro_t_now > 2.4f,
           "交叉污染阻断：Pyro counter/timer 必须保持 2/≈2.5（不被 Hydro 命中改动）",
           "Pyro_counter=%d（期望 2）  Pyro_timer=%.2f（期望 ≈2.5）", pyro_c_now, pyro_t_now);

    // —— 阶段 3：Hydro 再打 2 发（共 3 发，Hydro 序列 ✅❌❌）——
    bool H2 = icd_hit_allowed(icd, ElementType::Hydro, ElementStrength::Weak);
    bool H3 = icd_hit_allowed(icd, ElementType::Hydro, ElementStrength::Weak);
    EXPECT(H2 == false && H3 == false,
           "Hydro 第 2、3 发拦截（✅❌❌）",
           "H2=%d H3=%d H_counter=%d",
           (int)H2, (int)H3, peek(icd, ElementType::Hydro, ElementStrength::Weak).counter);

    // —— 阶段 4：回到 Pyro 打第 3 发 —— 仍应是 counter=3, 3%3=0 ❌
    bool P3 = icd_hit_allowed(icd, ElementType::Pyro, ElementStrength::Weak);
    EXPECT(P3 == false,
           "回到 Pyro 第 3 发：counter==3 → 3%3=0 ❌ 拦截（Hydro 的命中没帮 Pyro 前进计数）",
           "P3=%d Pyro_counter=%d", (int)P3,
           peek(icd, ElementType::Pyro, ElementStrength::Weak).counter);

    // —— 阶段 5：Pyro 第 4 发 —— counter=4 → 4%3=1 ✅（完全独立于 Hydro 的命中）
    bool P4 = icd_hit_allowed(icd, ElementType::Pyro, ElementStrength::Weak);
    EXPECT(P4 == true,
           "Pyro 第 4 发：counter==4 → 4%3=1 ✅（序列完整独立）",
           "P4=%d Pyro_counter=%d", (int)P4,
           peek(icd, ElementType::Pyro, ElementStrength::Weak).counter);

    // —— 额外：Strong(Anemo) 第 1 发 → 完全新组 ✅（含风岩验证）
    bool A1 = icd_hit_allowed(icd, ElementType::Anemo, ElementStrength::Strong);
    EXPECT(A1 == true,
           "Strong(Anemo) 第 1 发 ✅（Anemo 含在 7 元素中，独立分组）",
           "A1=%d AnemoStrong_counter=%d", (int)A1,
           peek(icd, ElementType::Anemo, ElementStrength::Strong).counter);
}

// ===========================================================================
// Main
// ===========================================================================
int main() {
    std::printf("=====================================================\n");
    std::printf(" ICD 单元测试（修复版：7 元素 × 2 档 = 14 组独立）\n");
    std::printf(" 分组 key：(触发元素 × 攻击档位)\n");
    std::printf("   每组各自规则：1,4,7,10… 放行 / 2.5s 到点清零 / 首次启动\n");
    std::printf(" 全局规则：每个敌人独立；ICD 拦截 → 反应也无效（选B）\n");
    std::printf("=====================================================\n");

    test_case1_weak_pyro_sequence();
    test_case2_timer_expiry_refresh();
    test_case3_gear_independent_same_element();
    test_case4_per_enemy_independent();
    test_case5_icd_blocks_reaction_too();
    test_case6_cross_element_independent();

    std::printf("\n================== SUMMARY ==================\n");
    std::printf("  PASS: %d\n", s_pass);
    std::printf("  FAIL: %d\n", s_fail);
    std::printf("  %s\n", s_fail == 0 ? "全部通过 🎉" : "有失败项 ❌");
    std::printf("===============================================\n");
    return s_fail == 0 ? 0 : 1;
}
