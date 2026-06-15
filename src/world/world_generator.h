#ifndef SPACETRAVELLER_WORLD_GENERATOR_H
#define SPACETRAVELLER_WORLD_GENERATOR_H

#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include "core/world_coords.h"
#include "core/rng.h"
#include "world/dungeon_generator.h"

namespace godot {

struct BiomeTile {
    uint16_t id;
    int weight;
};

struct BiomeInfo {
    std::vector<BiomeTile> ground_tiles;
    int total_weight = 0;
    std::unordered_map<uint16_t, uint16_t> fixed_overrides;
    bool auto_tiled = false;
    uint16_t border_tile_id = 0;
};

struct DungeonStructureContext {
    bool valid = false;
    String structure_id;
    Vector2i local_pos;
    int local_z = 0;
};

class WorldGenerator {
private:
    std::unordered_map<uint64_t, uint32_t> region_chunks;
    std::unordered_map<uint16_t, BiomeInfo> biome_rules;

    // Performance Cache: Last Chunk
    uint64_t last_chunk_key = 0;
    uint16_t last_chunk_id = 0;
    uint8_t last_chunk_rotation = 0;
    uint8_t last_chunk_neighbors = 0;
    const BiomeInfo* last_biome_ptr = nullptr;
    bool last_chunk_valid = false;

    // Data-Driven Registry IDs
    uint16_t id_void = 0;
    uint16_t id_air = 0;
    uint16_t id_building = 0;
    uint16_t id_forest = 0;
    uint16_t id_plains = 0;
    uint16_t id_underground_earth = 0;
    uint16_t id_solid_rock = 0;
    uint16_t id_road_bricks = 0;
    uint16_t id_road_flagstone = 0;
    uint16_t id_alley_bricks = 0;
    uint16_t id_alley_flagstone = 0;
    uint16_t id_crypt_entrance = 0;
    uint16_t id_dungeon_floor = 0;
    uint16_t id_dungeon_wall = 0;
    uint16_t id_dungeon_door = 0;

    std::unordered_map<uint64_t, DungeonLayout> dungeon_layout_cache;
    int dungeon_layout_cache_seed = 0;
    bool dungeon_layout_cache_seed_valid = false;
    struct DungeonEntranceRef {
        String dungeon_type;
        Vector2i entrance_chunk;
        int start_z = -1;
    };
    std::vector<DungeonEntranceRef> dungeon_entrance_cache;
    bool dungeon_entrance_cache_valid = false;

    // Pre-fetched singletons/references used during generation
    class StructureDb* s_db = nullptr;
    class IdRegistry* id_reg = nullptr;

    uint16_t get_base_surface_tile(int x, int y, int world_seed);
    uint16_t get_surface_feature_tile(int x, int y, uint16_t base_tile_id, int world_seed);
    uint16_t get_road_surface_feature_tile(int x, int y, uint16_t base_tile_id, int world_seed);
    uint16_t get_dungeon_tile(int x, int y, int z, int world_seed);
    DungeonLayout* get_or_create_dungeon_layout(const String& p_dungeon_type, const Vector2i& p_entrance_chunk, int p_world_seed);
    void reset_dungeon_cache();
    void rebuild_dungeon_entrance_cache();
    bool base_allows_surface_feature(uint16_t p_base_tile_id) const;
    bool validate_surface_feature_anchor(
        const String& p_feature_id,
        const Vector2i& p_origin,
        const Vector2i& p_source_size,
        const Vector2i& p_placed_size,
        uint8_t p_rotation,
        int p_world_seed
    );
    uint16_t get_surface_feature_tile_at(
        const String& p_feature_id,
        int p_local_x,
        int p_local_y,
        const Vector2i& p_source_size,
        uint8_t p_rotation
    ) const;

public:
    WorldGenerator();
    ~WorldGenerator();

    void setup_biome_rules();
    Dictionary init_region(const Vector2i& regionPos, int world_seed, const Ref<FastNoiseLite>& biome_noise);
    uint16_t get_tile(int x, int y, int world_seed);
    uint16_t get_tile(int x, int y, int z, int world_seed);
    uint16_t get_chunk_id_for_cell(int x, int y) const;
    uint8_t get_chunk_rotation_for_cell(int x, int y) const;
    String get_structure_id_for_chunk(uint16_t p_chunk_id) const;
    String get_structure_id_for_cell(int x, int y, int world_seed) const;
    DungeonStructureContext get_dungeon_structure_context(int x, int y, int z, int world_seed);
    bool is_dungeon_floor_loot_candidate(int x, int y, int z, int world_seed);
    
    void apply_auto_tiling(const Vector2i& p_region_pos);
    uint16_t pick_weighted_tile(const BiomeInfo& info, uint32_t hash);
    
    uint32_t get_hash(int x, int y, uint32_t seed) const {
        return static_cast<uint32_t>(Rng::hash_pos(seed, Vector2i(x, y), Rng::BIOME) >> 32);
    }

    // Accessors for GameWorld to handle saving/loading
    const std::unordered_map<uint64_t, uint32_t>& get_region_chunks() const { return region_chunks; }
    void set_region_chunks(const std::unordered_map<uint64_t, uint32_t>& chunks) { region_chunks = chunks; last_chunk_valid = false; reset_dungeon_cache(); }
    void clear_region_chunks() { region_chunks.clear(); last_chunk_valid = false; reset_dungeon_cache(); }
    void invalidate_cache() { last_chunk_valid = false; reset_dungeon_cache(); }
};

}

#endif // SPACETRAVELLER_WORLD_GENERATOR_H
