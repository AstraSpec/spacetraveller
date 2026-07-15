#ifndef SPACETRAVELLER_ORE_GENERATOR_H
#define SPACETRAVELLER_ORE_GENERATOR_H

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/vector3i.hpp>
#include <cstdint>
#include <cstddef>
#include <array>
#include <unordered_map>
#include <vector>

namespace godot {

struct OreFormationInfo;

struct OreSmallCell {
    uint64_t key = 0;
    uint16_t tile_id = 0;
};

struct OreCandidate {
    const OreFormationInfo* formation = nullptr;
    Vector2i source_chunk;
    Vector3i center;
    int slot = 0;
    float province_suitability = 0.0f;
    uint64_t salt = 0;

    int top_depth = 1;
    int thickness = 1;
    int length = 1;
    int width = 1;
    float cos_angle = 1.0f;
    float sin_angle = 0.0f;
    float richness = 1.0f;
    int band_phase = 0;
    int band_width = 1;

    std::vector<OreSmallCell> small_cells;
};

struct OreChunkDescriptors {
    std::vector<OreCandidate> candidates;
};

struct OreMatch {
    bool present = false;
    uint16_t tile_id = 0;
    const OreCandidate* candidate = nullptr;
};

class OreGenerator {
private:
    std::unordered_map<uint64_t, OreChunkDescriptors> descriptor_cache;
    int cached_seed = 0;
    bool cached_seed_valid = false;
    int cached_query_chunk_x = 0;
    int cached_query_chunk_y = 0;
    bool cached_query_neighborhood_valid = false;
    std::array<const OreChunkDescriptors*, 9> cached_query_neighborhood{};

    void ensure_seed(int p_world_seed);
    const OreChunkDescriptors& get_or_create_descriptors(int p_chunk_x, int p_chunk_y, int p_world_seed);
    OreChunkDescriptors build_descriptors(int p_chunk_x, int p_chunk_y, int p_world_seed) const;
    float sample_province(const OreFormationInfo& p_formation, int p_chunk_x, int p_chunk_y, int p_world_seed) const;
    bool candidate_matches_cell(const OreCandidate& p_candidate, int p_x, int p_y, int p_z, uint16_t& r_tile_id) const;
    uint16_t pick_weighted_mineral(const OreFormationInfo& p_formation, int p_roll) const;
    OreMatch find_match(int p_x, int p_y, int p_z, int p_world_seed);

public:
    OreGenerator() = default;

    uint16_t get_ore_tile(int p_x, int p_y, int p_z, int p_world_seed);
    Dictionary get_debug_info(int p_x, int p_y, int p_z, int p_world_seed);
    Array get_candidates_for_chunk(int p_chunk_x, int p_chunk_y, int p_world_seed);
    void clear_cache();
    size_t get_cached_chunk_count() const { return descriptor_cache.size(); }
};

}

#endif // SPACETRAVELLER_ORE_GENERATOR_H
