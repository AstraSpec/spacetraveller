#include "ore_generator.h"

#include "core/id_registry.h"
#include "core/rng.h"
#include "core/world_coords.h"
#include "data/ore_db.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace godot {

static constexpr double TAU = 6.28318530717958647692;

static int floor_div(int p_value, int p_divisor) {
    int quotient = p_value / p_divisor;
    int remainder = p_value % p_divisor;
    if (remainder != 0 && ((remainder < 0) != (p_divisor < 0))) quotient--;
    return quotient;
}

static int floor_mod(int p_value, int p_divisor) {
    int remainder = p_value % p_divisor;
    return remainder < 0 ? remainder + std::abs(p_divisor) : remainder;
}

static int floor_div_chunk(int p_value) {
    return floor_div(p_value, WorldCoords::CHUNK_SIZE);
}

static float smoothstep(float p_value) {
    return p_value * p_value * (3.0f - 2.0f * p_value);
}

static float lerp(float p_a, float p_b, float p_t) {
    return p_a + (p_b - p_a) * p_t;
}

static uint64_t formation_salt(const OreFormationInfo& p_formation, int p_slot) {
    uint64_t salt = static_cast<uint64_t>(p_formation.id.hash());
    salt ^= static_cast<uint64_t>(static_cast<uint32_t>(p_slot)) * 0x9E3779B97F4A7C15ULL;
    return Rng::mix64(salt);
}

void OreGenerator::ensure_seed(int p_world_seed) {
    if (cached_seed_valid && cached_seed == p_world_seed) return;
    descriptor_cache.clear();
    cached_query_neighborhood_valid = false;
    cached_seed = p_world_seed;
    cached_seed_valid = true;
}

void OreGenerator::clear_cache() {
    descriptor_cache.clear();
    cached_seed_valid = false;
    cached_query_neighborhood_valid = false;
}

float OreGenerator::sample_province(const OreFormationInfo& p_formation, int p_chunk_x, int p_chunk_y, int p_world_seed) const {
    const int scale = std::max(1, p_formation.province_scale_chunks);
    const int grid_x = floor_div(p_chunk_x, scale);
    const int grid_y = floor_div(p_chunk_y, scale);
    const float local_x = static_cast<float>(floor_mod(p_chunk_x, scale)) / static_cast<float>(scale);
    const float local_y = static_cast<float>(floor_mod(p_chunk_y, scale)) / static_cast<float>(scale);
    const float tx = smoothstep(local_x);
    const float ty = smoothstep(local_y);
    const uint64_t salt = static_cast<uint64_t>(p_formation.province_key.hash());

    auto corner = [&](int p_x, int p_y) {
        return Rng::at(static_cast<uint32_t>(p_world_seed), Vector2i(p_x, p_y), Rng::ORE_PROVINCE, salt).unit();
    };

    const float north = lerp(corner(grid_x, grid_y), corner(grid_x + 1, grid_y), tx);
    const float south = lerp(corner(grid_x, grid_y + 1), corner(grid_x + 1, grid_y + 1), tx);
    return lerp(north, south, ty);
}

uint16_t OreGenerator::pick_weighted_mineral(const OreFormationInfo& p_formation, int p_roll) const {
    if (p_formation.minerals.empty() || p_formation.total_mineral_weight <= 0) return 0;
    int roll = floor_mod(p_roll, p_formation.total_mineral_weight) + 1;
    for (const OreMineralInfo& mineral : p_formation.minerals) {
        if (roll <= mineral.cumulative_weight) return mineral.tile_id;
    }
    return p_formation.minerals.front().tile_id;
}

OreChunkDescriptors OreGenerator::build_descriptors(int p_chunk_x, int p_chunk_y, int p_world_seed) const {
    OreChunkDescriptors descriptors;
    OreDb* ore_db = OreDb::get_singleton();
    if (!ore_db) return descriptors;

    const Vector2i source_chunk(p_chunk_x, p_chunk_y);
    for (const OreFormationInfo& formation : ore_db->get_formations()) {
        const float suitability = sample_province(formation, p_chunk_x, p_chunk_y, p_world_seed);
        const float spawn_chance = suitability >= formation.province_threshold
            ? formation.inside_chance
            : formation.outside_chance;

        for (int slot = 0; slot < formation.candidate_slots; slot++) {
            const uint64_t salt = formation_salt(formation, slot);
            Rng::Seeded rng = Rng::at(static_cast<uint32_t>(p_world_seed), source_chunk, Rng::ORE_FORMATION, salt);
            if (!rng.chance(spawn_chance)) continue;

            OreCandidate candidate;
            candidate.formation = &formation;
            candidate.source_chunk = source_chunk;
            candidate.slot = slot;
            candidate.province_suitability = suitability;
            candidate.salt = salt;

            const int origin_x = p_chunk_x * WorldCoords::CHUNK_SIZE;
            const int origin_y = p_chunk_y * WorldCoords::CHUNK_SIZE;
            const int center_x = origin_x + rng.range(0, WorldCoords::CHUNK_SIZE - 1);
            const int center_y = origin_y + rng.range(0, WorldCoords::CHUNK_SIZE - 1);

            if (formation.type == "layered_lens") {
                candidate.top_depth = rng.range(formation.depth.min, formation.depth.max);
                candidate.thickness = rng.range(formation.thickness.min, formation.thickness.max);
                candidate.length = rng.range(formation.length.min, formation.length.max);
                candidate.width = rng.range(formation.width.min, formation.width.max);
                candidate.richness = lerp(formation.richness.min, formation.richness.max, rng.unit());
                candidate.band_width = rng.range(1, 2);
                candidate.band_phase = rng.range(0, std::max(0, formation.total_mineral_weight - 1));
                const double angle = static_cast<double>(rng.unit()) * TAU;
                candidate.cos_angle = static_cast<float>(std::cos(angle));
                candidate.sin_angle = static_cast<float>(std::sin(angle));
                const int center_depth = candidate.top_depth + (candidate.thickness - 1) / 2;
                candidate.center = Vector3i(center_x, center_y, -center_depth);
            } else {
                const int cell_count = rng.range(formation.cells.min, formation.cells.max);
                const int center_depth = rng.range(formation.depth.min, formation.depth.max);
                candidate.center = Vector3i(center_x, center_y, -center_depth);

                std::unordered_set<uint64_t> used;
                Vector3i cursor = candidate.center;
                const Vector3i directions[6] = {
                    Vector3i(1, 0, 0), Vector3i(-1, 0, 0),
                    Vector3i(0, 1, 0), Vector3i(0, -1, 0),
                    Vector3i(0, 0, 1), Vector3i(0, 0, -1)
                };
                int attempts = 0;
                while (static_cast<int>(candidate.small_cells.size()) < cell_count && attempts < cell_count * 32) {
                    attempts++;
                    const uint64_t key = WorldCoords::pack_coords_3d(cursor.x, cursor.y, cursor.z);
                    if (used.insert(key).second) {
                        const int mineral_roll = static_cast<int>(rng.next_u32() & 0x7FFFFFFFu);
                        candidate.small_cells.push_back({key, pick_weighted_mineral(formation, mineral_roll)});
                    }
                    const Vector3i next = cursor + directions[rng.range(0, 5)];
                    const int next_depth = -next.z;
                    if (next_depth >= formation.depth.min && next_depth <= formation.depth.max) cursor = next;
                }
                for (int offset = 1; static_cast<int>(candidate.small_cells.size()) < cell_count; offset++) {
                    const Vector3i fallback_cell = candidate.center + Vector3i(offset, 0, 0);
                    const uint64_t key = WorldCoords::pack_coords_3d(fallback_cell.x, fallback_cell.y, fallback_cell.z);
                    if (used.insert(key).second) {
                        const int mineral_roll = static_cast<int>(rng.next_u32() & 0x7FFFFFFFu);
                        candidate.small_cells.push_back({key, pick_weighted_mineral(formation, mineral_roll)});
                    }
                }
            }
            descriptors.candidates.push_back(std::move(candidate));
        }
    }
    return descriptors;
}

const OreChunkDescriptors& OreGenerator::get_or_create_descriptors(int p_chunk_x, int p_chunk_y, int p_world_seed) {
    ensure_seed(p_world_seed);
    const uint64_t key = WorldCoords::pack_coords(p_chunk_x, p_chunk_y);
    auto it = descriptor_cache.find(key);
    if (it != descriptor_cache.end()) return it->second;
    auto inserted = descriptor_cache.emplace(key, build_descriptors(p_chunk_x, p_chunk_y, p_world_seed));
    return inserted.first->second;
}

bool OreGenerator::candidate_matches_cell(
    const OreCandidate& p_candidate,
    int p_x,
    int p_y,
    int p_z,
    uint16_t& r_tile_id
) const {
    const OreFormationInfo& formation = *p_candidate.formation;

    if (formation.type == "random_walk") {
        const uint64_t key = WorldCoords::pack_coords_3d(p_x, p_y, p_z);
        for (const OreSmallCell& cell : p_candidate.small_cells) {
            if (cell.key == key) {
                r_tile_id = cell.tile_id;
                return r_tile_id != 0;
            }
        }
        return false;
    }

    const int depth = -p_z;
    if (p_z >= 0 || depth < p_candidate.top_depth || depth >= p_candidate.top_depth + p_candidate.thickness) return false;

    const float dx = static_cast<float>(p_x - p_candidate.center.x);
    const float dy = static_cast<float>(p_y - p_candidate.center.y);
    const float local_long = dx * p_candidate.cos_angle + dy * p_candidate.sin_angle;
    const float local_wide = -dx * p_candidate.sin_angle + dy * p_candidate.cos_angle;
    const float radius_long = std::max(0.5f, static_cast<float>(p_candidate.length - 1) * 0.5f);
    const float radius_wide = std::max(0.5f, static_cast<float>(p_candidate.width - 1) * 0.5f);
    if (std::abs(local_long) > radius_long || std::abs(local_wide) > radius_wide) return false;
    const float center_depth = static_cast<float>(p_candidate.top_depth) + (static_cast<float>(p_candidate.thickness) - 1.0f) * 0.5f;
    const float radius_depth = std::max(0.5f, static_cast<float>(p_candidate.thickness) * 0.5f);
    const float vertical = (static_cast<float>(depth) - center_depth) / radius_depth;
    const float horizontal = (local_long * local_long) / (radius_long * radius_long)
        + (local_wide * local_wide) / (radius_wide * radius_wide);

    const Vector3i cell_pos(p_x, p_y, p_z);
    Rng::Seeded cell_rng = Rng::at(static_cast<uint32_t>(cached_seed), cell_pos, Rng::ORE_CELL, p_candidate.salt);
    const float boundary_jitter = (cell_rng.unit() - 0.5f) * 0.20f;
    if (horizontal + vertical * vertical > 1.0f + boundary_jitter) return false;

    const float edge_factor = std::max(0.25f, 1.0f - 0.45f * horizontal);
    if (cell_rng.unit() >= p_candidate.richness * edge_factor) return false;

    const int band_coordinate = static_cast<int>(std::floor(local_wide)) + depth + p_candidate.band_phase;
    const int band = floor_div(band_coordinate, p_candidate.band_width);
    r_tile_id = pick_weighted_mineral(formation, band);
    return r_tile_id != 0;
}

static bool candidate_precedes(const OreCandidate& p_a, const OreCandidate& p_b) {
    if (p_a.formation->id != p_b.formation->id) return p_a.formation->id < p_b.formation->id;
    if (p_a.slot != p_b.slot) return p_a.slot < p_b.slot;
    if (p_a.source_chunk.x != p_b.source_chunk.x) return p_a.source_chunk.x < p_b.source_chunk.x;
    if (p_a.source_chunk.y != p_b.source_chunk.y) return p_a.source_chunk.y < p_b.source_chunk.y;
    return false;
}

OreMatch OreGenerator::find_match(int p_x, int p_y, int p_z, int p_world_seed) {
    OreMatch best;
    if (p_z >= 0) return best;
    const int chunk_x = floor_div_chunk(p_x);
    const int chunk_y = floor_div_chunk(p_y);
    ensure_seed(p_world_seed);
    if (!cached_query_neighborhood_valid || cached_query_chunk_x != chunk_x || cached_query_chunk_y != chunk_y) {
        int index = 0;
        for (int source_y = chunk_y - 1; source_y <= chunk_y + 1; source_y++) {
            for (int source_x = chunk_x - 1; source_x <= chunk_x + 1; source_x++) {
                cached_query_neighborhood[index++] = &get_or_create_descriptors(source_x, source_y, p_world_seed);
            }
        }
        cached_query_chunk_x = chunk_x;
        cached_query_chunk_y = chunk_y;
        cached_query_neighborhood_valid = true;
    }
    for (const OreChunkDescriptors* descriptors : cached_query_neighborhood) {
        if (!descriptors) continue;
        for (const OreCandidate& candidate : descriptors->candidates) {
            uint16_t tile_id = 0;
            if (!candidate_matches_cell(candidate, p_x, p_y, p_z, tile_id)) continue;
            if (!best.present || candidate_precedes(candidate, *best.candidate)) {
                best.present = true;
                best.tile_id = tile_id;
                best.candidate = &candidate;
            }
        }
    }
    return best;
}

uint16_t OreGenerator::get_ore_tile(int p_x, int p_y, int p_z, int p_world_seed) {
    return find_match(p_x, p_y, p_z, p_world_seed).tile_id;
}

Dictionary OreGenerator::get_debug_info(int p_x, int p_y, int p_z, int p_world_seed) {
    Dictionary result;
    OreMatch match = find_match(p_x, p_y, p_z, p_world_seed);
    result["present"] = match.present;
    if (!match.present || !match.candidate) return result;
    IdRegistry* id_reg = IdRegistry::get_singleton();
    result["formation_id"] = match.candidate->formation->id;
    result["winning_formation"] = match.candidate->formation->id;
    result["type"] = match.candidate->formation->type;
    result["tile_id"] = id_reg ? id_reg->get_string(match.tile_id) : String();
    result["mineral_tile"] = result["tile_id"];
    result["source_chunk"] = match.candidate->source_chunk;
    result["center"] = match.candidate->center;
    result["candidate_center"] = match.candidate->center;
    result["candidate_slot"] = match.candidate->slot;
    result["province_suitability"] = match.candidate->province_suitability;
    return result;
}

Array OreGenerator::get_candidates_for_chunk(int p_chunk_x, int p_chunk_y, int p_world_seed) {
    Array result;
    const OreChunkDescriptors& descriptors = get_or_create_descriptors(p_chunk_x, p_chunk_y, p_world_seed);
    for (const OreCandidate& candidate : descriptors.candidates) {
        Dictionary data;
        data["formation_id"] = candidate.formation->id;
        data["type"] = candidate.formation->type;
        data["source_chunk"] = candidate.source_chunk;
        data["center"] = candidate.center;
        data["candidate_slot"] = candidate.slot;
        data["province_suitability"] = candidate.province_suitability;
        if (candidate.formation->type == "layered_lens") {
            data["top_depth"] = candidate.top_depth;
            data["thickness"] = candidate.thickness;
            data["length"] = candidate.length;
            data["width"] = candidate.width;
            data["richness"] = candidate.richness;
        } else {
            data["cell_count"] = static_cast<int>(candidate.small_cells.size());
        }
        result.push_back(data);
    }
    return result;
}

}
