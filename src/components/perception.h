#ifndef SPACETRAVELLER_PERCEPTION_H
#define SPACETRAVELLER_PERCEPTION_H

#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <cstdint>
#include <unordered_set>

namespace godot {

class WorldBubble;
class TileDb;
struct Entity;

enum class PerceptionTier {
    NONE,
    RAYCAST,
    FULL_OCCLUSION
};

struct PerceptionMemory {
    std::unordered_set<uint64_t> known_tiles;
    std::unordered_set<uint64_t> known_entities;
    Vector2i last_known_player_pos;
    bool player_seen = false;
};

namespace Perception {
    void tick_full(PerceptionMemory& mem, const Entity& self,
                   const WorldBubble& bubble, const Vector2i& player_pos,
                   int sight_radius);

    void tick_raycast(PerceptionMemory& mem, const Entity& self,
                      const Vector2i& target_pos,
                      const WorldBubble& bubble, const TileDb& tile_db);

    bool has_line_of_sight(int x1, int y1, int x2, int y2,
                           const WorldBubble& bubble, const TileDb& tile_db);

    Dictionary serialize(const PerceptionMemory& mem);
    void deserialize(PerceptionMemory& mem, const Dictionary& dict);
}

}

#endif // SPACETRAVELLER_PERCEPTION_H
