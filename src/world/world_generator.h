#ifndef SPACETRAVELLER_WORLD_GENERATOR_H
#define SPACETRAVELLER_WORLD_GENERATOR_H

#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include "core/world_coords.h"
#include "core/rng.h"

namespace godot {

struct BiomeTile {
    uint16_t id;
    int weight;
};

struct BiomeInfo {
    std::vector<BiomeTile> ground_tiles;
    std::unordered_map<uint16_t, uint16_t> fixed_overrides;
    bool auto_tiled = false;
    uint16_t border_tile_id = 0;
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
    uint16_t id_building = 0;
    uint16_t id_forest = 0;
    uint16_t id_plains = 0;

    // Pre-fetched singletons/references used during generation
    class StructureDb* s_db = nullptr;
    class IdRegistry* id_reg = nullptr;

public:
    WorldGenerator();
    ~WorldGenerator();

    void setup_biome_rules();
    Dictionary init_region(const Vector2i& regionPos, int world_seed, const Ref<FastNoiseLite>& biome_noise);
    uint16_t get_tile(int x, int y, int world_seed);
    uint16_t get_chunk_id_for_cell(int x, int y) const;
    uint8_t get_chunk_rotation_for_cell(int x, int y) const;
    String get_structure_id_for_chunk(uint16_t p_chunk_id) const;
    
    void apply_auto_tiling(const Vector2i& p_region_pos);
    uint16_t pick_weighted_tile(const BiomeInfo& info, uint32_t roll);
    
    uint32_t get_hash(int x, int y, uint32_t seed) const {
        return static_cast<uint32_t>(Rng::hash_pos(seed, Vector2i(x, y), Rng::BIOME) >> 32);
    }

    // Accessors for GameWorld to handle saving/loading
    const std::unordered_map<uint64_t, uint32_t>& get_region_chunks() const { return region_chunks; }
    void set_region_chunks(const std::unordered_map<uint64_t, uint32_t>& chunks) { region_chunks = chunks; last_chunk_valid = false; }
    void clear_region_chunks() { region_chunks.clear(); last_chunk_valid = false; }
    void invalidate_cache() { last_chunk_valid = false; }
};

}

#endif // SPACETRAVELLER_WORLD_GENERATOR_H
