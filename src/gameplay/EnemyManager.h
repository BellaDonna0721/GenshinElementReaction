#pragma once
#include "../core/World.h"
#include "SpawnPreset.h"
#include <vector>

// =====================================================================
//  EnemyManager —— 敌人管理器（gameplay 层辅助类，非 ECS System）
//
//  职责：
//    · 提供敌人生成 / 移除 / 清空 / 计数接口，供关卡脚本调用
//    · 跟踪当前存活的敌人实体列表（用于胜利条件判定）
//    · DamageSystem 检测到敌人死亡时回调 on_enemy_died 统一销毁+注销
//
//  为什么不做对象池：
//    敌人生成频率低、种类多、生命周期长，直接 create/destroy 即可，
//    不像子弹需要高频复用。
//
//  自定义关卡扩展：
//    未来加新敌人类型只需在 SpawnPreset 里加预设 + 扩展 preset_id 映射。
// =====================================================================

class EnemyManager {
public:
    EnemyManager() = default;

    // 生成敌人：调用 spawn_enemy 工厂，并注册到内部存活列表
    // 返回生成的实体 ID（失败返回 INVALID_ENTITY）
    Entity spawn(World& world, const EnemyPreset& preset, float x, float y);

    // 移除指定敌人（关卡脚本手动移除）
    void remove(World& world, Entity e);

    // 清空所有敌人（关卡切换 / 重置）
    void clear_all(World& world);

    // 存活敌人数量（胜利条件判定用）
    int alive_count() const { return static_cast<int>(m_enemies.size()); }

    // DamageSystem 回调：某敌人 HP 归零 → 销毁实体 + 从存活列表注销
    void on_enemy_died(World& world, Entity e);

private:
    // 从存活列表中移除某个实体（不销毁，由调用方决定）
    void unregister(Entity e);

    std::vector<Entity> m_enemies;  // 当前存活的敌人实体列表
};

extern EnemyManager g_enemy_manager;  // 全局实例（wasm_api / DamageSystem 共享）
