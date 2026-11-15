#include "LevelLoader.h"
#include "../utils/GameConfig.h"
#include "../importFiles/nlohmann/json.hpp"
#include <utility>

using nlohmann::json;

static std::vector<LevelConfig> g_levels;

const std::vector<LevelConfig>& get_levels() {
    return g_levels;
}

void set_levels(std::vector<LevelConfig> levels) {
    g_levels = std::move(levels);
}

std::vector<LevelConfig> parse_levels_json(const std::string& text) {
    json root = json::parse(text);
    std::vector<LevelConfig> levels;
    levels.reserve(root.size());

    for (const auto& jl : root) {
        LevelConfig cfg;
        cfg.level_id = jl.value("level_id", 0);
        cfg.is_boss  = jl.value("is_boss", false);

        // 玩家出生点（归一化 → 像素）
        if (jl.contains("player_spawn")) {
            cfg.player_spawn.x = GameConfig::to_world_x(jl["player_spawn"].value("x", 0.0f));
            cfg.player_spawn.y = GameConfig::to_world_y(jl["player_spawn"].value("y", 0.0f));
        }

        // 墙体（中心坐标 + 宽高均归一化）
        if (jl.contains("walls")) {
            for (const auto& jw : jl["walls"]) {
                WallDef wd;
                wd.x = GameConfig::to_world_x(jw.value("x", 0.0f));
                wd.y = GameConfig::to_world_y(jw.value("y", 0.0f));
                wd.w = jw.value("w", 0.0f) * GameConfig::WORLD_W;
                wd.h = jw.value("h", 0.0f) * GameConfig::WORLD_H;
                cfg.walls.push_back(wd);
            }
        }

        // 波次
        if (jl.contains("waves")) {
            for (const auto& jw : jl["waves"]) {
                WaveConfig wc;
                wc.delay = jw.value("delay", 0.0f);
                if (jw.contains("spawns")) {
                    for (const auto& js : jw["spawns"]) {
                        EnemySpawn sp;
                        sp.preset_id = js.value("preset_id", 0);
                        sp.x = GameConfig::to_world_x(js.value("x", 0.0f));
                        sp.y = GameConfig::to_world_y(js.value("y", 0.0f));
                        wc.spawns.push_back(sp);
                    }
                }
                cfg.waves.push_back(wc);
            }
        }

        levels.push_back(std::move(cfg));
    }

    return levels;
}
