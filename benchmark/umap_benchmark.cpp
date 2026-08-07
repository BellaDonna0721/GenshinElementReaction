#include <chrono>
#include <cstdio>
#include <random>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <utility>
#include "../src/components/ElementStatus.h"
#include "../src/importFiles/myUnorderedMap.hpp"

// ========== 防编译器过度优化：全局 volatile sink + 真正的运行时熵源 ==========
// 写入必须是"运行时依赖"的，不能让编译器在编译期就推导出是常数。
volatile size_t g_bench_sink = 0;

// Escape hatch：一个 volatile 指针数组。把任意局部容器的内部地址写进这里，
// GCC 就无法证明该容器"完全是局部的、没有对外副作用"，从而无法做堆分配消除。
// (slot数量=质数，减少next_salt低位取模的冲突)
static volatile void* volatile g_bench_escape[31] = {nullptr};

static inline void escape_ptr(void* p) {
    size_t slot = (size_t)p % 31u;
    g_bench_escape[slot] = p;
}

// 真正的运行时熵源：CPU cycle counter。GCC 无法在编译期推导出 rdtsc 的值。
#if defined(_MSC_VER)
#  include <intrin.h>
#  pragma intrinsic(__rdtsc)
static inline size_t rdtsc_entropy() { return (size_t)__rdtsc(); }
#elif defined(__GNUC__) || defined(__clang__)
#  if defined(__i386__) || defined(__x86_64__)
static inline size_t rdtsc_entropy() {
    unsigned lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((size_t)hi << 32) | lo;
}
#  else
static inline size_t rdtsc_entropy() {
    return (size_t)std::chrono::high_resolution_clock::now()
                          .time_since_epoch().count();
}
#  endif
#else
static inline size_t rdtsc_entropy() {
    return (size_t)std::chrono::high_resolution_clock::now()
                          .time_since_epoch().count();
}
#endif

// 每轮都用不同的 salt 让 digest 跨轮变化。使用 rdtsc 运行时熵作为 seed，
// 再和 volatile g_bench_sink 形成"真·运行时依赖链"——GCC 绝对无法在编译期求值。
static size_t next_salt() {
    size_t t = rdtsc_entropy();
    g_bench_sink ^= t + 0x9E3779B97F4A7C15ULL;
    return g_bench_sink ^ t;
}

// ============== Test ReactionRule data ==============
struct TestReactionRule {
    ElementType trigger;
    ElementType target;
    ReactionType result;
    bool consumes_target;
    bool consumes_trigger;
    float target_cost_per_trigger;
};

static TestReactionRule g_test_rules[] = {
    // Vaporize
    {ElementType::Hydro, ElementType::Pyro,   ReactionType::Vaporize,        true, true, 2.0f},
    {ElementType::Pyro,  ElementType::Hydro,  ReactionType::ReverseVaporize, true, true, 0.5f},
    // Melt
    {ElementType::Pyro,  ElementType::Cryo,   ReactionType::Melt,            true, true, 2.0f},
    {ElementType::Cryo,  ElementType::Pyro,   ReactionType::ReverseMelt,     true, true, 0.5f},
    // Overload
    {ElementType::Pyro,  ElementType::Electro,ReactionType::Overload,        true, true, 1.0f},
    {ElementType::Electro, ElementType::Pyro, ReactionType::Overload,        true, true, 1.0f},
    // ElectroCharged
    {ElementType::Electro, ElementType::Hydro,ReactionType::ElectroCharged,  true, true, 1.0f},
    {ElementType::Hydro, ElementType::Electro,ReactionType::ElectroCharged,  true, true, 1.0f},
    // Frozen
    {ElementType::Cryo,  ElementType::Hydro,  ReactionType::Frozen,          true, true, 1.0f},
    {ElementType::Hydro, ElementType::Cryo,   ReactionType::Frozen,          true, true, 1.0f},
    // Superconduct
    {ElementType::Cryo,  ElementType::Electro,ReactionType::Superconduct,    true, true, 1.0f},
    {ElementType::Electro,ElementType::Cryo,  ReactionType::Superconduct,    true, true, 1.0f},
    // Swirl
    {ElementType::Anemo, ElementType::Pyro,   ReactionType::Swirl,           true, true, 1.0f},
    {ElementType::Anemo, ElementType::Hydro,  ReactionType::Swirl,           true, true, 1.0f},
    {ElementType::Anemo, ElementType::Electro,ReactionType::Swirl,           true, true, 1.0f},
    {ElementType::Anemo, ElementType::Cryo,   ReactionType::Swirl,           true, true, 1.0f},
    // Crystallize
    {ElementType::Geo,   ElementType::Pyro,   ReactionType::Crystallize,     true, true, 1.0f},
    {ElementType::Geo,   ElementType::Hydro,  ReactionType::Crystallize,     true, true, 1.0f},
    {ElementType::Geo,   ElementType::Electro,ReactionType::Crystallize,     true, true, 1.0f},
    {ElementType::Geo,   ElementType::Cryo,   ReactionType::Crystallize,     true, true, 1.0f},
    // Burning
    {ElementType::Dendro,ElementType::Pyro,   ReactionType::Burning,         true, true, 1.0f},
    {ElementType::Pyro,  ElementType::Dendro, ReactionType::Burning,         true, true, 1.0f},
    // Bloom
    {ElementType::Dendro,ElementType::Hydro,  ReactionType::Bloom,           true, true, 1.0f},
    {ElementType::Hydro, ElementType::Dendro, ReactionType::Bloom,           true, true, 1.0f},
};
static constexpr int g_rule_count = sizeof(g_test_rules) / sizeof(g_test_rules[0]);

// ============== Key + custom Hash ==============
using RuleKey = std::pair<ElementType, ElementType>;

struct RuleKeyHash {
    size_t operator()(const RuleKey& k) const noexcept {
        return (static_cast<size_t>(static_cast<uint8_t>(k.first))  << 8)
             |  static_cast<size_t>(static_cast<uint8_t>(k.second));
    }
};

// ============== Three lookup implementations ==============
static const TestReactionRule* find_linear(ElementType trigger, ElementType target) {
    for (int i = 0; i < g_rule_count; ++i) {
        if (g_test_rules[i].trigger == trigger && g_test_rules[i].target == target) {
            return &g_test_rules[i];
        }
    }
    return nullptr;
}

static myUnorderedMap<RuleKey, const TestReactionRule*, RuleKeyHash> g_my_umap;
static const TestReactionRule* find_my_umap(ElementType trigger, ElementType target) {
    auto* p = g_my_umap.find(RuleKey{trigger, target});
    return p ? p->second : nullptr;
}

static std::unordered_map<RuleKey, const TestReactionRule*, RuleKeyHash> g_std_umap;
static const TestReactionRule* find_std_umap(ElementType trigger, ElementType target) {
    auto it = g_std_umap.find(RuleKey{trigger, target});
    return (it != g_std_umap.end()) ? it->second : nullptr;
}

static void init_maps() {
    for (int i = 0; i < g_rule_count; ++i) {
        RuleKey k{g_test_rules[i].trigger, g_test_rules[i].target};
        g_my_umap.insert_or_assign(k, &g_test_rules[i]);
    }
    for (int i = 0; i < g_rule_count; ++i) {
        RuleKey k{g_test_rules[i].trigger, g_test_rules[i].target};
        g_std_umap[k] = &g_test_rules[i];
    }
}

static std::vector<std::pair<ElementType, ElementType>> generate_queries(size_t N) {
    std::vector<std::pair<ElementType, ElementType>> queries;
    queries.reserve(N);
    std::mt19937 rng(42);
    std::vector<ElementType> all_elements = {
        ElementType::Pyro, ElementType::Hydro, ElementType::Electro,
        ElementType::Cryo, ElementType::Anemo, ElementType::Geo, ElementType::Dendro
    };
    for (size_t i = 0; i < N; ++i) {
        if (rng() % 10 < 7) {
            int idx = rng() % g_rule_count;
            queries.emplace_back(g_test_rules[idx].trigger, g_test_rules[idx].target);
        } else {
            ElementType a = all_elements[rng() % all_elements.size()];
            ElementType b = all_elements[rng() % all_elements.size()];
            queries.emplace_back(a, b);
        }
    }
    return queries;
}

using Clock = std::chrono::high_resolution_clock;
using Nano  = std::chrono::nanoseconds;

template<typename F>
static double benchmark_one(const char* name, F&& func,
                            const std::vector<std::pair<ElementType, ElementType>>& queries,
                            int iterations) {
    volatile size_t dummy = 0;
    size_t salt = next_salt();
    for (auto& q : queries) {
        auto* r = func(q.first, q.second);
        dummy += (r ? (size_t)r->result : 0) ^ salt;
    }
    auto t0 = Clock::now();
    for (int it = 0; it < iterations; ++it) {
        size_t s = next_salt();
        for (auto& q : queries) {
            auto* r = func(q.first, q.second);
            dummy += ((r ? (size_t)r->result : 0) ^ s);
        }
    }
    auto t1 = Clock::now();
    g_bench_sink ^= dummy;
    auto total_ns = std::chrono::duration_cast<Nano>(t1 - t0).count();
    size_t total_ops = (size_t)iterations * queries.size();
    double ns_per_op = (double)total_ns / (double)total_ops;
    printf("  %-24s | total=%9lld ns | avg=%7.2f ns/op | Mops/s=%8.2f\n",
           name, (long long)total_ns, ns_per_op, 1000.0 / ns_per_op);
    return ns_per_op;
}

static void verify_correctness(const std::vector<std::pair<ElementType, ElementType>>& queries) {
    int mismatches = 0;
    for (auto& q : queries) {
        auto* r1 = find_linear    (q.first, q.second);
        auto* r2 = find_my_umap   (q.first, q.second);
        auto* r3 = find_std_umap  (q.first, q.second);
        ReactionType rt1 = r1 ? r1->result : ReactionType::None;
        ReactionType rt2 = r2 ? r2->result : ReactionType::None;
        ReactionType rt3 = r3 ? r3->result : ReactionType::None;
        if (rt1 != rt2) { mismatches++;
            printf("  [ERROR] myUnorderedMap mismatch: t=%d tg=%d  lin=%d my=%d\n",
                   (int)q.first,(int)q.second,(int)rt1,(int)rt2); }
        if (rt1 != rt3) { mismatches++;
            printf("  [ERROR] std::unordered_map mismatch: t=%d tg=%d  lin=%d std=%d\n",
                   (int)q.first,(int)q.second,(int)rt1,(int)rt3); }
    }
    if (mismatches == 0) printf("  [OK] All three lookups produce identical results\n");
    else                 printf("  [FAIL] %d mismatches found\n", mismatches);
}

static void print_memory_layout() {
    printf("\n============ Memory Layout & Pointer Addresses ============\n");
    RuleKey test_key{ElementType::Hydro, ElementType::Pyro};
    auto* r_raw = find_linear(ElementType::Hydro, ElementType::Pyro);
    printf("  raw-array rule address:    %p\n", (void*)r_raw);
    auto* p_my = g_my_umap.find(test_key);
    printf("  myUmap pair address:       %p  (inner: key=%p value_ptr=%p)\n",
           (void*)p_my, (void*)&p_my->first, (void*)&p_my->second);
    printf("  myUmap actual rule ptr:    %p\n", (void*)p_my->second);
    auto it_std = g_std_umap.find(test_key);
    printf("  stdUmap node address:      %p  (inner: key=%p value_ptr=%p)\n",
           (void*)&(*it_std), (void*)&it_std->first, (void*)&it_std->second);
    printf("  stdUmap actual rule ptr:   %p\n", (void*)it_std->second);
    printf("\n  [Cache impact] In myUnorderedMap, pairs in the same bucket are stored\n");
    printf("  contiguously in a vector.  std::unordered_map scatters linked-list\n");
    printf("  nodes across the heap.  Contiguous storage = fewer cache misses.\n");
}

// ==============================================================
//  动态操作（增/删/改）
// ==============================================================

struct LinearRuleSet {
    std::vector<TestReactionRule> rules;

    const TestReactionRule* find(ElementType trigger, ElementType target) const {
        for (auto& r : rules)
            if (r.trigger == trigger && r.target == target) return &r;
        return nullptr;
    }

    // 语义：与 map::insert_or_assign 对等 —— 已存在 key 则覆盖全值，不存在则追加
    void insert_or_assign(const TestReactionRule& rule) {
        for (auto& r : rules) {
            if (r.trigger == rule.trigger && r.target == rule.target) {
                r = rule;
                return;
            }
        }
        rules.push_back(rule);
    }

    bool erase(ElementType trigger, ElementType target) {
        for (auto it = rules.begin(); it != rules.end(); ++it) {
            if (it->trigger == trigger && it->target == target) {
                rules.erase(it);
                return true;
            }
        }
        return false;
    }

    bool update(ElementType trigger, ElementType target, float new_cost) {
        for (auto& r : rules) {
            if (r.trigger == trigger && r.target == target) {
                r.target_cost_per_trigger = new_cost;
                return true;
            }
        }
        return false;
    }

    size_t size() const { return rules.size(); }
};

static std::vector<TestReactionRule> generate_unique_rules_49() {
    std::vector<TestReactionRule> rules;
    rules.reserve(49);
    for (int i = 0; i < 7; ++i) {
        for (int j = 0; j < 7; ++j) {
            ElementType a = static_cast<ElementType>(i + 1);
            ElementType b = static_cast<ElementType>(j + 1);
            rules.push_back({a, b, ReactionType::None, true, true, 1.0f});
        }
    }
    return rules;
}

// 混合写入流：前 49 条 = 全新 key；后续 = 随机挑已有 key（覆盖更新约 75%）
// 关键：每条的 result 字段写了不同枚举值，防编译器常量传播。
static std::vector<TestReactionRule> generate_mixed_insert_stream(size_t N) {
    std::vector<TestReactionRule> rules;
    rules.reserve(N);
    std::mt19937 rng(123);
    auto base49 = generate_unique_rules_49();
    for (size_t i = 0; i < 49 && i < N; ++i) {
        TestReactionRule r = base49[i];
        // result 写入不同值，确保每条规则有独特的"运行时特征"
        r.result = static_cast<ReactionType>((i * 3 + 1) % 32 + 1);
        rules.push_back(r);
    }
    for (size_t i = 49; i < N; ++i) {
        size_t idx = rng() % 49;
        TestReactionRule r = base49[idx];
        r.result = static_cast<ReactionType>((i * 7 + 3) % 32 + 1);
        r.target_cost_per_trigger = 0.5f + float(rng() % 100) * 0.03f;
        rules.push_back(r);
    }
    return rules;
}

static void verify_mutation_correctness() {
    printf("\n============ Mutation Correctness Check ============\n");
    int failures = 0;
    auto base49 = generate_unique_rules_49();

    LinearRuleSet lin;
    myUnorderedMap<RuleKey, TestReactionRule, RuleKeyHash> mym;
    std::unordered_map<RuleKey, TestReactionRule, RuleKeyHash> stdm;

    for (auto& r : base49) {
        RuleKey k{r.trigger, r.target};
        lin.insert_or_assign(r);
        mym.insert_or_assign(k, r);
        stdm[k] = r;
    }
    if (lin.size() != 49) { printf("  [FAIL] Linear insert size=%zu (want 49)\n", lin.size()); failures++; }
    if (mym.size() != 49) { printf("  [FAIL] myUm insert size=%zu (want 49)\n", mym.size()); failures++; }
    if (stdm.size() != 49){ printf("  [FAIL] stdUm insert size=%zu (want 49)\n", stdm.size()); failures++; }

    for (int i = 0; i < 20; ++i) {
        auto& r = base49[(size_t)i * 3 % 49];
        RuleKey k{r.trigger, r.target};
        auto* L = lin.find(r.trigger, r.target);
        auto* M = mym.find(k);
        auto  S  = stdm.find(k);
        bool ok = L && M && S != stdm.end();
        if (!ok) { printf("  [FAIL] sample find #%d mismatch\n", i); failures++; }
    }

    for (auto& r : base49) {
        TestReactionRule updated = r;
        updated.target_cost_per_trigger = 3.5f;
        RuleKey k{r.trigger, r.target};
        lin.insert_or_assign(updated);
        mym.insert_or_assign(k, updated);
        stdm[k] = updated;
    }
    {
        RuleKey k = RuleKey{base49[0].trigger, base49[0].target};
        auto* L = lin.find(base49[0].trigger, base49[0].target);
        auto* M = mym.find(k);
        auto  S = stdm.find(k);
        if (!L || L->target_cost_per_trigger != 3.5f) { printf("  [FAIL] Linear update value wrong\n"); failures++; }
        if (!M || M->second.target_cost_per_trigger != 3.5f) { printf("  [FAIL] myUm update value wrong\n"); failures++; }
        if (S == stdm.end() || S->second.target_cost_per_trigger != 3.5f) { printf("  [FAIL] stdUm update value wrong\n"); failures++; }
    }

    for (int i = 0; i < 12; ++i) {
        auto& r = base49[i];
        RuleKey k{r.trigger, r.target};
        lin.erase(r.trigger, r.target);
        mym.erase(k);
        stdm.erase(k);
    }
    size_t want = 49 - 12;
    if (lin.size() != want) { printf("  [FAIL] Linear after erase size=%zu (want %zu)\n", lin.size(), want); failures++; }
    if (mym.size() != want) { printf("  [FAIL] myUm after erase size=%zu (want %zu)\n", mym.size(), want); failures++; }
    if (stdm.size()!= want) { printf("  [FAIL] stdUm after erase size=%zu (want %zu)\n", stdm.size(),want); failures++; }

    if (failures == 0) printf("  [OK] Insert / Erase / Update all pass correctness check\n");
    else               printf("  [FAIL] %d correctness issues in mutation ops\n", failures);
}

// ==============================================================
//  插入 benchmark
// ==============================================================
static void run_insert_benchmarks() {
    constexpr int PerRound   = 200;
    constexpr int Iterations = 200;
    auto stream = generate_mixed_insert_stream(PerRound);

    printf("\n============ Insert (insert_or_assign) Performance ============\n");
    printf("  模式：每轮 = 空容器 → %d 条写入(49条全新 + 151条覆盖)  × %d 轮\n", PerRound, Iterations);
    printf("  三者语义完全对等：存在 key 则更新值，不存在才插入；最终 size 均 = 49\n");
    printf("  %-24s | %13s | %15s | %10s\n",
           "Implementation","Total ns","ns / write","M writes/s");
    printf("  "); for(int i=0;i<82;++i) printf("-"); printf("\n");

    // 1) Linear
    {
        size_t digest = next_salt();
        auto t0 = Clock::now();
        for (int it = 0; it < Iterations; ++it) {
            LinearRuleSet container;
            size_t s = next_salt();
            for (size_t k = 0; k < stream.size(); ++k) {
                auto& r = stream[k];
                container.insert_or_assign(r);
                escape_ptr(container.rules.data());
                digest ^= (size_t)container.rules.data()
                        ^ (size_t)r.result
                        ^ (k << 3)
                        ^ (container.size() * s);
            }
        }
        auto t1 = Clock::now();
        g_bench_sink ^= digest;
        digest = 0;
        for (int i = 0; i < 31; ++i) digest ^= (size_t)g_bench_escape[i];
        g_bench_sink ^= digest;
        auto total_ns = std::chrono::duration_cast<Nano>(t1 - t0).count();
        double ns_per = (double)total_ns / (double)(PerRound * Iterations);
        printf("  %-24s | total=%9lld ns | avg=%7.2f ns/op | Mops/s=%8.2f\n",
               "1) Linear (dedup)", (long long)total_ns, ns_per, 1000.0 / ns_per);
    }

    // 2) myUnorderedMap
    {
        size_t digest = next_salt();
        auto t0 = Clock::now();
        for (int it = 0; it < Iterations; ++it) {
            myUnorderedMap<RuleKey, TestReactionRule, RuleKeyHash> container;
            size_t s = next_salt();
            for (size_t k = 0; k < stream.size(); ++k) {
                auto& r = stream[k];
                RuleKey key{r.trigger, r.target};
                container.insert_or_assign(key, r);
                auto* rp = container.find(key);
                escape_ptr((void*)rp);
                digest ^= (size_t)rp
                        ^ (size_t)r.result
                        ^ (k << 3)
                        ^ (container.size() * s);
            }
        }
        auto t1 = Clock::now();
        g_bench_sink ^= digest;
        digest = 0;
        for (int i = 0; i < 31; ++i) digest ^= (size_t)g_bench_escape[i];
        g_bench_sink ^= digest;
        auto total_ns = std::chrono::duration_cast<Nano>(t1 - t0).count();
        double ns_per = (double)total_ns / (double)(PerRound * Iterations);
        printf("  %-24s | total=%9lld ns | avg=%7.2f ns/op | Mops/s=%8.2f\n",
               "2) myUnorderedMap", (long long)total_ns, ns_per, 1000.0 / ns_per);
    }

    // 3) std::unordered_map
    {
        size_t digest = next_salt();
        auto t0 = Clock::now();
        for (int it = 0; it < Iterations; ++it) {
            std::unordered_map<RuleKey, TestReactionRule, RuleKeyHash> container;
            size_t s = next_salt();
            for (size_t k = 0; k < stream.size(); ++k) {
                auto& r = stream[k];
                RuleKey key{r.trigger, r.target};
                container[key] = r;
                auto itrp = container.find(key);
                void* p = (itrp != container.end()) ? (void*)&(*itrp) : nullptr;
                escape_ptr(p);
                digest ^= (size_t)p
                        ^ (size_t)r.result
                        ^ (k << 3)
                        ^ (container.size() * s);
            }
        }
        auto t1 = Clock::now();
        g_bench_sink ^= digest;
        digest = 0;
        for (int i = 0; i < 31; ++i) digest ^= (size_t)g_bench_escape[i];
        g_bench_sink ^= digest;
        auto total_ns = std::chrono::duration_cast<Nano>(t1 - t0).count();
        double ns_per = (double)total_ns / (double)(PerRound * Iterations);
        printf("  %-24s | total=%9lld ns | avg=%7.2f ns/op | Mops/s=%8.2f\n",
               "3) std::unordered_map", (long long)total_ns, ns_per, 1000.0 / ns_per);
    }
}

// ==============================================================
//  删除 benchmark
// ==============================================================
static void run_delete_benchmarks() {
    std::vector<RuleKey> delete_existing;
    for (int i = 0; i < g_rule_count; ++i) {
        delete_existing.emplace_back(g_test_rules[i].trigger, g_test_rules[i].target);
    }
    // 关键：shuffle 的 RNG 种子依赖 g_bench_sink（volatile 全局变量，运行时才有确定值），
    // 阻止编译器在编译期推导出删除顺序，从而无法常量折叠整个循环。
    std::mt19937 rng_shuf(static_cast<uint32_t>(g_bench_sink ^ 0x55AA55AAu));
    std::shuffle(delete_existing.begin(), delete_existing.end(), rng_shuf);

    constexpr int Rounds = 500;
    constexpr int OpsPerRound = g_rule_count;

    printf("\n============ Deletion Performance (total ~%d ops) ============\n",
           OpsPerRound * Rounds);
    printf("  模式：每轮 = 建表(%d条) + 删除全部%d条key(随机序，100%%命中)  × %d 轮\n",
           g_rule_count, OpsPerRound, Rounds);
    printf("  %-24s | %13s | %15s | %10s\n",
           "Implementation","Total ns","ns / delete","M deletes/s");
    printf("  "); for(int i=0;i<82;++i) printf("-"); printf("\n");

    // 1) Linear
    {
        size_t digest = next_salt();
        auto t0 = Clock::now();
        for (int it = 0; it < Rounds; ++it) {
            LinearRuleSet container;
            for (auto& r : g_test_rules) container.insert_or_assign(r);
            // 发布内部地址：让 GCC 无法做 Heap Allocation Elimination
            escape_ptr(container.rules.data());
            size_t s = next_salt();
            for (size_t k = 0; k < delete_existing.size(); ++k) {
                auto& key = delete_existing[k];
                bool ok = container.erase(key.first, key.second);
                // 发布新的内部地址（erase 可能导致 shrink/move）
                escape_ptr(container.rules.data());
                // 读回尾元素字节，强制副作用落地
                uint32_t tail_bits = 0x1234;
                if (!container.rules.empty()) {
                    auto& back = container.rules.back();
                    tail_bits = *(uint32_t*)&back.target_cost_per_trigger
                              ^ (uint32_t)back.trigger
                              ^ ((uint32_t)back.target << 16);
                }
                digest ^= (size_t)container.rules.data()
                        ^ (size_t)tail_bits
                        ^ (ok ? 0xA5u : 0x5Au)
                        ^ ((size_t)container.size() << 2)
                        ^ (k * s);
            }
        }
        auto t1 = Clock::now();
        g_bench_sink ^= digest;
        // 最后再从 escape 数组里读一个值回来，确保写入可见
        digest = 0;
        for (int i = 0; i < 31; ++i) digest ^= (size_t)g_bench_escape[i];
        g_bench_sink ^= digest;
        auto total_ns = std::chrono::duration_cast<Nano>(t1 - t0).count();
        double ns_per = (double)total_ns / (double)(OpsPerRound * Rounds);
        printf("  %-24s | total=%9lld ns | avg=%7.2f ns/op | Mops/s=%8.2f\n",
               "1) Linear (dedup)", (long long)total_ns, ns_per, 1000.0 / ns_per);
    }

    // 2) myUnorderedMap
    {
        size_t digest = next_salt();
        auto t0 = Clock::now();
        for (int it = 0; it < Rounds; ++it) {
            myUnorderedMap<RuleKey, TestReactionRule, RuleKeyHash> container;
            for (auto& r : g_test_rules) {
                RuleKey k{r.trigger, r.target};
                container.insert_or_assign(k, r);
            }
            size_t s = next_salt();
            for (size_t k = 0; k < delete_existing.size(); ++k) {
                auto& key = delete_existing[k];
                bool ok = container.erase(key);
                auto probe = container.find(delete_existing[0]);
                escape_ptr((void*)probe);
                digest ^= (size_t)probe
                        ^ (ok ? 0xA5u : 0x5Au)
                        ^ ((size_t)container.size() << 2)
                        ^ (k * s);
            }
        }
        auto t1 = Clock::now();
        g_bench_sink ^= digest;
        digest = 0;
        for (int i = 0; i < 31; ++i) digest ^= (size_t)g_bench_escape[i];
        g_bench_sink ^= digest;
        auto total_ns = std::chrono::duration_cast<Nano>(t1 - t0).count();
        double ns_per = (double)total_ns / (double)(OpsPerRound * Rounds);
        printf("  %-24s | total=%9lld ns | avg=%7.2f ns/op | Mops/s=%8.2f\n",
               "2) myUnorderedMap", (long long)total_ns, ns_per, 1000.0 / ns_per);
    }

    // 3) std::unordered_map
    {
        size_t digest = next_salt();
        auto t0 = Clock::now();
        for (int it = 0; it < Rounds; ++it) {
            std::unordered_map<RuleKey, TestReactionRule, RuleKeyHash> container;
            for (auto& r : g_test_rules) {
                RuleKey k{r.trigger, r.target};
                container[k] = r;
            }
            size_t s = next_salt();
            for (size_t k = 0; k < delete_existing.size(); ++k) {
                auto& key = delete_existing[k];
                auto it_f = container.find(key);
                bool ok = false;
                if (it_f != container.end()) {
                    container.erase(it_f);
                    ok = true;
                }
                auto probe = container.find(delete_existing[0]);
                void* pp = (probe != container.end()) ? (void*)&(*probe) : nullptr;
                escape_ptr(pp);
                digest ^= (size_t)pp
                        ^ (ok ? 0xA5u : 0x5Au)
                        ^ ((size_t)container.size() << 2)
                        ^ (k * s);
            }
        }
        auto t1 = Clock::now();
        g_bench_sink ^= digest;
        digest = 0;
        for (int i = 0; i < 31; ++i) digest ^= (size_t)g_bench_escape[i];
        g_bench_sink ^= digest;
        auto total_ns = std::chrono::duration_cast<Nano>(t1 - t0).count();
        double ns_per = (double)total_ns / (double)(OpsPerRound * Rounds);
        printf("  %-24s | total=%9lld ns | avg=%7.2f ns/op | Mops/s=%8.2f\n",
               "3) std::unordered_map", (long long)total_ns, ns_per, 1000.0 / ns_per);
    }
}

// ==============================================================
//  更新 benchmark
// ==============================================================
static void run_update_benchmarks() {
    std::vector<RuleKey> update_keys;
    for (int i = 0; i < g_rule_count; ++i) {
        update_keys.emplace_back(g_test_rules[i].trigger, g_test_rules[i].target);
    }
    // 同样用 g_bench_sink 污染 RNG 种子，让 key 顺序编译期不可知
    std::mt19937 rng_shuf(static_cast<uint32_t>(g_bench_sink ^ 0x33CC33CCu));
    std::shuffle(update_keys.begin(), update_keys.end(), rng_shuf);

    constexpr int Rounds = 500;
    constexpr int OpsPerRound = g_rule_count;

    printf("\n============ Update Performance (total ~%d ops) ============\n",
           OpsPerRound * Rounds);
    printf("  模式：每轮 = 建表(%d条) + 更新全部%d条key(随机序，100%%命中)  × %d 轮\n",
           g_rule_count, OpsPerRound, Rounds);
    printf("  %-24s | %13s | %15s | %10s\n",
           "Implementation","Total ns","ns / update","M updates/s");
    printf("  "); for(int i=0;i<82;++i) printf("-"); printf("\n");

    // 1) Linear
    {
        size_t digest = next_salt();
        auto t0 = Clock::now();
        for (int it = 0; it < Rounds; ++it) {
            LinearRuleSet container;
            for (auto& r : g_test_rules) container.insert_or_assign(r);
            escape_ptr(container.rules.data());
            size_t s = next_salt();
            for (size_t k = 0; k < update_keys.size(); ++k) {
                auto& key = update_keys[k];
                // new_cost 完全依赖运行时 salt，不可在编译期预测
                float new_cost = 1.0f + 0.01f * float(next_salt() & 0xFF);
                bool ok = container.update(key.first, key.second, new_cost);
                escape_ptr(container.rules.data());
                // 关键：更新后立刻读回这个元素的值，强制写入不被省略
                auto* rp = container.find(key.first, key.second);
                escape_ptr((void*)rp);
                float read_back = rp ? rp->target_cost_per_trigger : 0.0f;
                digest ^= (size_t)container.rules.data()
                        ^ (size_t)rp
                        ^ *(uint32_t*)&read_back
                        ^ (ok ? 0xC3u : 0x3Cu)
                        ^ ((size_t)container.size() << 4)
                        ^ (k * s);
            }
        }
        auto t1 = Clock::now();
        g_bench_sink ^= digest;
        // escape 读回
        digest = 0;
        for (int i = 0; i < 31; ++i) digest ^= (size_t)g_bench_escape[i];
        g_bench_sink ^= digest;
        auto total_ns = std::chrono::duration_cast<Nano>(t1 - t0).count();
        double ns_per = (double)total_ns / (double)(OpsPerRound * Rounds);
        printf("  %-24s | total=%9lld ns | avg=%7.2f ns/op | Mops/s=%8.2f\n",
               "1) Linear (dedup)", (long long)total_ns, ns_per, 1000.0 / ns_per);
    }

    // 2) myUnorderedMap
    {
        size_t digest = next_salt();
        auto t0 = Clock::now();
        for (int it = 0; it < Rounds; ++it) {
            myUnorderedMap<RuleKey, TestReactionRule, RuleKeyHash> container;
            for (auto& r : g_test_rules) {
                RuleKey k{r.trigger, r.target};
                container.insert_or_assign(k, r);
            }
            size_t s = next_salt();
            for (size_t k = 0; k < update_keys.size(); ++k) {
                auto& key = update_keys[k];
                float new_cost = 1.0f + 0.01f * float(next_salt() & 0xFF);
                auto* p = const_cast<std::pair<RuleKey, TestReactionRule>*>(container.find(key));
                bool ok = false;
                if (p) {
                    p->second.target_cost_per_trigger = new_cost;
                    ok = true;
                }
                // 写后读：强制写入实际落地
                auto* rp = container.find(key);
                escape_ptr((void*)rp);
                float read_back = rp ? rp->second.target_cost_per_trigger : 0.0f;
                digest ^= (size_t)rp
                        ^ *(uint32_t*)&read_back
                        ^ (ok ? 0xC3u : 0x3Cu)
                        ^ ((size_t)container.size() << 4)
                        ^ (k * s);
            }
        }
        auto t1 = Clock::now();
        g_bench_sink ^= digest;
        digest = 0;
        for (int i = 0; i < 31; ++i) digest ^= (size_t)g_bench_escape[i];
        g_bench_sink ^= digest;
        auto total_ns = std::chrono::duration_cast<Nano>(t1 - t0).count();
        double ns_per = (double)total_ns / (double)(OpsPerRound * Rounds);
        printf("  %-24s | total=%9lld ns | avg=%7.2f ns/op | Mops/s=%8.2f\n",
               "2) myUnorderedMap", (long long)total_ns, ns_per, 1000.0 / ns_per);
    }

    // 3) std::unordered_map
    {
        size_t digest = next_salt();
        auto t0 = Clock::now();
        for (int it = 0; it < Rounds; ++it) {
            std::unordered_map<RuleKey, TestReactionRule, RuleKeyHash> container;
            for (auto& r : g_test_rules) {
                RuleKey k{r.trigger, r.target};
                container[k] = r;
            }
            size_t s = next_salt();
            for (size_t k = 0; k < update_keys.size(); ++k) {
                auto& key = update_keys[k];
                float new_cost = 1.0f + 0.01f * float(next_salt() & 0xFF);
                auto it_f = container.find(key);
                bool ok = false;
                if (it_f != container.end()) {
                    it_f->second.target_cost_per_trigger = new_cost;
                    ok = true;
                }
                auto it_p = container.find(key);
                void* pp = (it_p != container.end()) ? (void*)&(*it_p) : nullptr;
                escape_ptr(pp);
                float read_back = (it_p != container.end()) ? it_p->second.target_cost_per_trigger : 0.0f;
                digest ^= (size_t)pp
                        ^ *(uint32_t*)&read_back
                        ^ (ok ? 0xC3u : 0x3Cu)
                        ^ ((size_t)container.size() << 4)
                        ^ (k * s);
            }
        }
        auto t1 = Clock::now();
        g_bench_sink ^= digest;
        digest = 0;
        for (int i = 0; i < 31; ++i) digest ^= (size_t)g_bench_escape[i];
        g_bench_sink ^= digest;
        auto total_ns = std::chrono::duration_cast<Nano>(t1 - t0).count();
        double ns_per = (double)total_ns / (double)(OpsPerRound * Rounds);
        printf("  %-24s | total=%9lld ns | avg=%7.2f ns/op | Mops/s=%8.2f\n",
               "3) std::unordered_map", (long long)total_ns, ns_per, 1000.0 / ns_per);
    }
}

// ============== main ==============
int main() {
    printf("======== Element Reaction Lookup & Mutation Benchmark ========\n");
    printf("Rules: %d,  Key-space: 7 x 7 = 49 possible combinations\n\n", g_rule_count);

    init_maps();

    printf("============ Correctness Check (Lookup) ============\n");
    { auto q = generate_queries(10000); verify_correctness(q); }

    verify_mutation_correctness();

    constexpr int N = 100000;
    auto queries = generate_queries(N);
    printf("\n============ Lookup Performance (%d queries x 100 iters) ============\n", N);
    printf("  %-24s | %13s | %15s | %10s\n",
           "Implementation","Total ns","ns / query","M queries/s");
    printf("  "); for(int i=0;i<82;++i) printf("-"); printf("\n");

    double ns_lin = benchmark_one("1) Linear Scan (24 rules)", find_linear,   queries, 100);
    double ns_my  = benchmark_one("2) myUnorderedMap",         find_my_umap,  queries, 100);
    double ns_std = benchmark_one("3) std::unordered_map",     find_std_umap, queries, 100);

    printf("\n============ Speedup (Linear = 1.00x) ============\n");
    printf("  Linear Scan:        1.000x (baseline)\n");
    printf("  myUnorderedMap:     %.3fx  %s\n", ns_lin/ns_my,  ns_my  < ns_lin ? "[FASTER]" : "[slower]");
    printf("  std::unordered_map: %.3fx  %s\n", ns_lin/ns_std, ns_std < ns_lin ? "[FASTER]" : "[slower]");
    printf("\n  myUnorderedMap vs std::unordered_map: %.3fx\n", ns_std/ns_my);
    if (ns_my < ns_std)
        printf("    -> Hand-written version is %.1f%% faster than STL (contiguous storage)\n",
               (1.0 - ns_my/ns_std)*100);
    else
        printf("    -> STL is %.1f%% faster\n", (1.0 - ns_std/ns_my)*100);

    run_insert_benchmarks();
    run_delete_benchmarks();
    run_update_benchmarks();
    print_memory_layout();

    printf("\n[bench sink = %llu] (anti-DCE anchor, ignore)\n", (unsigned long long)g_bench_sink);

    printf("\n============ Recommendation ============\n");
    if (ns_my < ns_lin && ns_my < ns_std) {
        printf("  [GO] Use myUnorderedMap in ElementReactionSystem::find_rule.\n");
    } else if (ns_lin < ns_my) {
        printf("  [KEEP] With only ~24 rules, linear scan has the lowest constant.\n");
        printf("         Re-benchmark once the rule count exceeds ~100.\n");
    } else {
        printf("  [UP TO YOU] myUnorderedMap beats linear scan but vs STL is close.\n");
    }
    return 0;
}
