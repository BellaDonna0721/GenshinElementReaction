#pragma once
#include "../components/GameState.h"
#include <string>

// =====================================================================
//  BuffRegistry —— 增益注册表（纯静态查表，无状态）
//
//  集中管理所有增益的元数据：名称、描述、数值。
//  各系统（InputSystem/DamageSystem/MovementSystem）通过这里查询增益效果。
//  新增增益只需在此表加一行 + BuffType 枚举加一项。
// =====================================================================

struct BuffInfo {
    BuffType    type;
    const char* name;        // 显示名称
    const char* description; // 显示描述
    float       value;       // 数值参数（倍率或固定值，语义由 type 决定）
};

class BuffRegistry {
public:
    // 根据类型获取增益信息
    static const BuffInfo& get(BuffType type);

    // 增益总数
    static constexpr int count() { return static_cast<int>(BuffType::Count); }

private:
    static const BuffInfo s_table[static_cast<int>(BuffType::Count)];
};
