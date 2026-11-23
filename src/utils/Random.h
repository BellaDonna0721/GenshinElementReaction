#pragma once
#include <random>
#include <chrono>
#include <cstdint>

// =====================================================================
//  game_rng —— 全局随机数工具
//
//  统一用 <random> 替代全局 rand()，每次运行序列都不同
//  （旧 rand() 无 srand，种子恒为 1，巡逻选点/增益抽取每次一样）。
//
//  【为什么不用 std::random_device 播种】
//  Emscripten(WASM) 下 random_device 读 /dev/urandom → getentropy →
//  调用 JS 的 crypto.getRandomValues(heapU8.subarray(...))。heapU8 是
//  WASM 线性内存的视图，而项目开了 ALLOW_MEMORY_GROWTH，该内存是
//  可扩容(resizable)的。Chrome 127+/新版 Edge 禁止 getRandomValues 接收
//  resizable 视图，会抛 TypeError → 游戏循环中断、画面卡住。
//  改用高精度时钟播种，绕开该限制，对游戏随机数足够。
//  （benchmark/umap_benchmark.cpp 用固定种子保证可复现，不走本工具）
// =====================================================================
namespace game_rng {

// 计算一个每次运行都不同的 32 位种子（取当前纳秒时间戳，高/低 32 位异或折叠）
inline std::uint32_t make_seed() {
    const auto ns = std::chrono::high_resolution_clock::now().time_since_epoch();
    const std::uint64_t ticks = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(ns).count());
    return static_cast<std::uint32_t>(ticks ^ (ticks >> 32));
}

// 引擎：thread_local，多线程下各线程序列独立
// 只在首次调用时用 make_seed() 播种一次，之后全程复用同一个引擎
inline std::mt19937& engine() {
    static thread_local std::mt19937 rng(make_seed());
    return rng;
}

// [lo, hi) 均匀浮点
inline float range(float lo, float hi) {
    return std::uniform_real_distribution<float>(lo, hi)(engine());
}

// [lo, hi] 闭区间均匀整数
inline int range_int(int lo, int hi) {
    return std::uniform_int_distribution<int>(lo, hi)(engine());
}

}  // namespace game_rng
