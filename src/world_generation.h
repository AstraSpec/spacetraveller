#ifndef SPACETRAVELLER_WORLD_GENERATION_H
#define SPACETRAVELLER_WORLD_GENERATION_H

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/rect2i.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cmath>
#include <godot_cpp/variant/utility_functions.hpp>
#include "occlusion.h"
#include "city_generation.h"
#include "data/tile_db.h"
#include "data/chunk_db.h"
#include "data/item_db.h"
#include "data/inventory.h"

#include "fast_tilemap.h"

namespace godot {

struct BiomeTile {
    uint16_t id;
    int weight;
};

struct DroppedItem {
    uint16_t id;
    int amount;
};

struct BiomeInfo {
    std::vector<BiomeTile> ground_tiles;
    // Map for specific overrides (e.g. chunk_id -> fixed_tile_id)
    std::unordered_map<uint16_t, uint16_t> fixed_overrides;
};

class WorldGeneration : public FastTileMap {
    GDCLASS(WorldGeneration, FastTileMap)

public:
    static constexpr uint32_t ROTATION_MASK = 0x03;
    static constexpr uint32_t ORIENTATION_SHIFT = 16;
    static constexpr uint32_t ID_MASK = 0xFFFF;

    enum {
        ROT_SOUTH = 0,
        ROT_WEST = 1,
        ROT_NORTH = 2,
        ROT_EAST = 3
    };

private:
    // Constants
    static const int REGION_SIZE = 256;
    static const int CHUNK_SIZE = 24;

    std::unordered_map<uint64_t, uint32_t> region_chunks; // Packed: [Rot][ID]
    std::unordered_map<uint64_t, std::vector<DroppedItem>> dropped_items;
    
    // Performance Cache: Last Chunk
    uint64_t last_chunk_key = 0;
    uint16_t last_chunk_id = 0;
    uint8_t last_chunk_rotation = 0;
    const BiomeInfo* last_biome_ptr = nullptr;
    bool last_chunk_valid = false;
    
    // Pre-fetched singletons
    class StructureDb* s_db = nullptr;
    class IdRegistry* id_reg = nullptr;
    
    // References set from GDScript
    Ref<FastNoiseLite> biome_noise;
    int world_seed = 0;
    
    // Data-Driven Registry
    uint16_t id_void = 0;
    uint16_t id_building = 0;
    uint16_t id_forest = 0;
    uint16_t id_plains = 0;
    std::unordered_map<uint16_t, BiomeInfo> biome_rules;
    
    // Helpers
    uint32_t get_hash(int x, int y, uint32_t seed) const {
        return (static_cast<uint32_t>(x) * 1597334677U) ^ 
               (static_cast<uint32_t>(y) * 3812015801U) ^ 
               (seed);
    }
    uint16_t get_tile(int x, int y);
    uint16_t pick_weighted_tile(const BiomeInfo& info, uint32_t roll);
    void setup_biome_rules();

protected:
    static void _bind_methods();

public:
    WorldGeneration();
    ~WorldGeneration();
    
    static int get_region_size() { return REGION_SIZE; }
    static int get_chunk_size() { return CHUNK_SIZE; }

    static uint64_t pack_coords(int x, int y) { return Occlusion::pack_coords(x, y); }
    static Vector2i unpack_coords(uint64_t key) {
        return Vector2i(
            static_cast<int>(static_cast<int32_t>(key >> 32)),
            static_cast<int>(static_cast<int32_t>(key & 0xFFFFFFFF))
        );
    }
    
    void set_biome_noise(const Ref<FastNoiseLite>& noise);
    Ref<FastNoiseLite> get_biome_noise() const;
    void set_world_seed(int seed);
    int get_world_seed() const;
    
    void update_world_bubble(const Vector2i& playerPos);
    Dictionary init_region(const Vector2i& regionPos);
    void drop_item(const Vector2i& pos, const String& item_id, int amount);
    bool pickup_item(const Vector2i& pos, Inventory* p_inventory);
};

}

#endif // ! SPACETRAVELLER_WORLD_GENERATION_H
