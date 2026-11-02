#include "Levels.h"
#include "../components/Transform.h"

// preset_id: 0=Pyro 1=Hydro 2=Cryo 3=Boss
namespace {

// 第 1 关：基础关，三只水史莱姆
LevelConfig make_level_1() {
    LevelConfig cfg;
    cfg.level_id = 1;
    cfg.is_boss  = false;
    cfg.player_spawn = Transform{150.0f, 350.0f};

    cfg.walls = {
        {420.0f, 200.0f, 30.0f, 220.0f},
        {530.0f, 420.0f, 200.0f, 30.0f},
    };

    WaveConfig w1;
    w1.delay = 1.0f;
    w1.spawns = {
        {1, 600.0f, 200.0f, 1, 0.0f},
        {1, 700.0f, 350.0f, 1, 0.5f},
        {1, 600.0f, 500.0f, 1, 0.5f},
    };
    cfg.waves = {w1};
    return cfg;
}

// 第 2 关：Boss 关，一个 Boss
LevelConfig make_level_2() {
    LevelConfig cfg;
    cfg.level_id = 2;
    cfg.is_boss  = true;
    cfg.player_spawn = Transform{150.0f, 350.0f};

    cfg.walls = {
        {350.0f, 150.0f, 30.0f, 150.0f},
        {350.0f, 550.0f, 30.0f, 150.0f},
        {600.0f, 350.0f, 200.0f, 30.0f},
    };

    WaveConfig w1;
    w1.delay = 1.0f;
    w1.spawns = {
        {3, 750.0f, 350.0f, 1, 0.0f},  // Boss
    };
    cfg.waves = {w1};
    return cfg;
}

// 第 3 关：基础关，火丘丘 + 水史莱姆混合
LevelConfig make_level_3() {
    LevelConfig cfg;
    cfg.level_id = 3;
    cfg.is_boss  = false;
    cfg.player_spawn = Transform{150.0f, 350.0f};

    cfg.walls = {
        {400.0f, 100.0f, 30.0f, 180.0f},
        {400.0f, 600.0f, 30.0f, 180.0f},
        {700.0f, 350.0f, 30.0f, 200.0f},
    };

    WaveConfig w1;
    w1.delay = 1.0f;
    w1.spawns = {
        {0, 550.0f, 200.0f, 1, 0.0f},   // 火
        {1, 550.0f, 500.0f, 1, 0.5f},   // 水
        {0, 800.0f, 350.0f, 1, 0.5f},   // 火
    };
    cfg.waves = {w1};
    return cfg;
}

// 第 4 关：Boss 关
LevelConfig make_level_4() {
    LevelConfig cfg;
    cfg.level_id = 4;
    cfg.is_boss  = true;
    cfg.player_spawn = Transform{150.0f, 350.0f};

    cfg.walls = {
        {300.0f, 200.0f, 180.0f, 30.0f},
        {300.0f, 500.0f, 180.0f, 30.0f},
        {650.0f, 350.0f, 30.0f, 250.0f},
    };

    WaveConfig w1;
    w1.delay = 1.0f;
    w1.spawns = {
        {3, 800.0f, 350.0f, 1, 0.0f},
    };
    cfg.waves = {w1};
    return cfg;
}

} // namespace

const std::vector<LevelConfig>& get_builtin_levels() {
    static const std::vector<LevelConfig> levels = {
        make_level_1(),
        make_level_2(),
        make_level_3(),
        make_level_4(),
    };
    return levels;
}
