#include "BuffRegistry.h"

// 增益表：顺序与 BuffType 枚举严格对应
const BuffInfo BuffRegistry::s_table[static_cast<int>(BuffType::Count)] = {
    { BuffType::AttackUp,        "攻击强化", "攻击力 +20%",             0.20f },
    { BuffType::AttackSpeedUp,   "疾速射击", "攻击速度 +15%",            0.15f },
    { BuffType::MoveSpeedUp,     "疾风步",   "移动速度 +15%",            0.15f },
    { BuffType::MaxHpUp,         "强健体魄", "生命上限 +30 并回满",      30.0f },
    { BuffType::Heal,            "治愈之光", "立即回复 50% 生命值",      0.50f },
    { BuffType::ElementDamageUp, "元素精通", "元素反应伤害 +25%",        0.25f },
    { BuffType::BulletDamageUp,  "穿透弹",   "子弹伤害 +5",              5.0f  },
    { BuffType::Shield,          "元素护盾", "获得一层护盾（抵挡一次伤害）", 1.0f },
    { BuffType::MasteryUp,       "元素洞悉", "元素精通 +5",              5.0f  },
};

const BuffInfo& BuffRegistry::get(BuffType type) {
    return s_table[static_cast<int>(type)];
}
