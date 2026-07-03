#include "light_map.h"

#include "core/world_coords.h"
#include "data/tile_db.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <queue>

using namespace godot;

namespace {

struct LightCell {
    Vector2i offset;
    uint64_t offset_key = 0;
    uint64_t cell_key = 0;
    uint16_t tile_id = 0;
    bool passes_light = true;
    bool aperture = false;
    bool sky_exposed = false;
};

bool tile_passes_natural_light(const TileInfo* p_info) {
    return !p_info || !p_info->solid || p_info->transparent;
}

bool tile_is_light_aperture(const TileInfo* p_info) {
    return p_info && p_info->closes_to != 0 && (!p_info->solid || p_info->transparent);
}

void raise_faces_for_step(LightSample& p_sample, const Vector2i& p_step, LightStrength p_strength) {
    if (p_step.x > 0) {
        p_sample.raise_face(LightFace::West, p_strength);
    } else if (p_step.x < 0) {
        p_sample.raise_face(LightFace::East, p_strength);
    }

    if (p_step.y > 0) {
        p_sample.raise_face(LightFace::North, p_strength);
    } else if (p_step.y < 0) {
        p_sample.raise_face(LightFace::South, p_strength);
    }
}

bool line_has_direct_light(
    const Vector2i& p_from,
    const Vector2i& p_to,
    const std::vector<LightCell>& p_cells,
    const std::unordered_map<uint64_t, size_t>& p_index_by_offset,
    Vector2i* r_final_step = nullptr
) {
    if (p_from == p_to) {
        if (r_final_step) {
            *r_final_step = Vector2i();
        }
        return true;
    }

    int x0 = p_from.x;
    int y0 = p_from.y;
    const int x1 = p_to.x;
    const int y1 = p_to.y;
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (x0 != x1 || y0 != y1) {
        const int previous_x = x0;
        const int previous_y = y0;
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }

        if (x0 == x1 && y0 == y1) {
            if (r_final_step) {
                *r_final_step = Vector2i(x0 - previous_x, y0 - previous_y);
            }
            return true;
        }

        const uint64_t offset_key = WorldCoords::pack_coords(x0, y0);
        auto it = p_index_by_offset.find(offset_key);
        if (it == p_index_by_offset.end() || !p_cells[it->second].passes_light) {
            return false;
        }
    }

    return true;
}

LightStrength strength_for_distance(int p_distance, LightStrength p_source_strength) {
    if (p_source_strength <= LIGHT_STRENGTH_BLANK) {
        return LIGHT_STRENGTH_BLANK;
    }

    const int strength = static_cast<int>(p_source_strength) - (p_distance * LIGHT_STRENGTH_FALLOFF_PER_TILE);
    return strength > 0 ? static_cast<LightStrength>(strength) : LIGHT_STRENGTH_BLANK;
}

int circular_tile_distance(int p_dx, int p_dy) {
    const int64_t dx = p_dx;
    const int64_t dy = p_dy;
    const int64_t distance_sq = dx * dx + dy * dy;
    if (distance_sq <= 0) {
        return 0;
    }

    return static_cast<int>(std::sqrt(static_cast<double>(distance_sq)));
}

void spill_from_source(
    const LightCell& p_source,
    LightStrength p_source_strength,
    int p_max_radius,
    const std::vector<LightCell>& p_cells,
    const std::unordered_map<uint64_t, size_t>& p_index_by_offset,
    std::vector<LightSample>& r_samples
) {
    if (p_source_strength <= LIGHT_STRENGTH_BLANK) {
        return;
    }

    const int source_radius = static_cast<int>(p_source_strength - LIGHT_STRENGTH_LOW);
    const int radius = std::min(source_radius, std::max(0, p_max_radius));
    const double ellipse_radius = static_cast<double>(radius) + 0.5;
    const double ellipse_radius_sq = ellipse_radius * ellipse_radius;
    for (int dy = -radius; dy <= radius; dy++) {
        const double row_center_y = static_cast<double>(dy);
        const double row_s = 1.0 - (row_center_y * row_center_y) / ellipse_radius_sq;
        if (row_s < 0.0) {
            continue;
        }

        const double row_extent = ellipse_radius * std::sqrt(row_s);
        const int min_dx = static_cast<int>(std::ceil(-row_extent));
        const int max_dx = static_cast<int>(std::floor(row_extent));

        for (int dx = min_dx; dx <= max_dx; dx++) {
            const int ox = p_source.offset.x + dx;
            const int oy = p_source.offset.y + dy;
            const int distance = circular_tile_distance(dx, dy);

            const uint64_t offset_key = WorldCoords::pack_coords(ox, oy);
            auto target_it = p_index_by_offset.find(offset_key);
            if (target_it == p_index_by_offset.end()) {
                continue;
            }

            const size_t target_index = target_it->second;
            Vector2i final_step;
            if (!line_has_direct_light(p_source.offset, p_cells[target_index].offset, p_cells, p_index_by_offset, &final_step)) {
                continue;
            }

            const LightStrength candidate = strength_for_distance(distance, p_source_strength);
            if (candidate <= LIGHT_STRENGTH_BLANK) {
                continue;
            }

            if (p_cells[target_index].passes_light) {
                r_samples[target_index].raise_cell(candidate);
            } else if (distance == 0) {
                r_samples[target_index].raise_all_faces(candidate);
            } else {
                raise_faces_for_step(r_samples[target_index], final_step, candidate);
            }
        }
    }
}

}

void LightMap::compute_natural_light(
    const Vector2i& p_origin,
    int p_z,
    const std::vector<uint64_t>& p_offset_keys,
    const TileResolver& p_resolve_tile,
    const SkyResolver& p_is_sky_exposed,
    const std::vector<LightEmitter>& p_emitters,
    std::unordered_map<uint64_t, LightSample>& r_samples
) {
    r_samples.clear();
    r_samples.reserve(p_offset_keys.size());

    if (p_offset_keys.empty() || !p_resolve_tile || !p_is_sky_exposed) {
        return;
    }

    TileDb* tile_db = TileDb::get_singleton();

    std::vector<LightCell> cells;
    cells.reserve(p_offset_keys.size());
    std::unordered_map<uint64_t, size_t> index_by_offset;
    index_by_offset.reserve(p_offset_keys.size());

    for (uint64_t offset_key : p_offset_keys) {
        Vector2i offset = WorldCoords::unpack_coords(offset_key);
        const int world_x = p_origin.x + offset.x;
        const int world_y = p_origin.y + offset.y;
        const uint16_t tile_id = p_resolve_tile(world_x, world_y, p_z);
        const TileInfo* info = tile_db ? tile_db->get_tile_info(tile_id) : nullptr;

        LightCell cell;
        cell.offset = offset;
        cell.offset_key = offset_key;
        cell.cell_key = WorldCoords::pack_coords_3d(world_x, world_y, p_z);
        cell.tile_id = tile_id;
        cell.passes_light = tile_passes_natural_light(info);
        cell.aperture = tile_is_light_aperture(info);
        cell.sky_exposed = p_is_sky_exposed(world_x, world_y, p_z);

        index_by_offset[offset_key] = cells.size();
        cells.push_back(cell);
        r_samples[cell.cell_key] = LightSample();
    }

    int light_area_radius = 0;
    for (const LightCell& cell : cells) {
        light_area_radius = std::max(light_area_radius, circular_tile_distance(cell.offset.x, cell.offset.y));
    }

    std::vector<LightSample> local_samples(cells.size());

    if (p_z >= 0) {
        std::vector<size_t> aperture_sources;
        std::queue<size_t> queue;

        for (size_t i = 0; i < cells.size(); i++) {
            const LightCell& cell = cells[i];
            if (!cell.passes_light || !cell.sky_exposed || cell.aperture) {
                continue;
            }
            local_samples[i].cell = LIGHT_STRENGTH_DAYLIGHT;
            queue.push(i);
        }

        static const Vector2i DIRECTIONS[4] = {
            Vector2i(1, 0),
            Vector2i(-1, 0),
            Vector2i(0, 1),
            Vector2i(0, -1)
        };

        while (!queue.empty()) {
            const size_t index = queue.front();
            queue.pop();

            const LightCell& cell = cells[index];
            const LightStrength current = local_samples[index].cell;
            if (current <= LIGHT_STRENGTH_BLANK) {
                continue;
            }

            for (const Vector2i& direction : DIRECTIONS) {
                const Vector2i neighbour_offset = cell.offset + direction;
                const uint64_t neighbour_key = WorldCoords::pack_coords(neighbour_offset.x, neighbour_offset.y);
                auto neighbour_it = index_by_offset.find(neighbour_key);
                if (neighbour_it == index_by_offset.end()) {
                    continue;
                }

                const size_t neighbour_index = neighbour_it->second;
                const LightCell& neighbour = cells[neighbour_index];
                const LightStrength candidate = current;

                if (!neighbour.passes_light) {
                    raise_faces_for_step(local_samples[neighbour_index], direction, candidate);
                    continue;
                }

                if (candidate > local_samples[neighbour_index].cell) {
                    local_samples[neighbour_index].cell = candidate;
                    if (neighbour.aperture || !neighbour.sky_exposed) {
                        aperture_sources.push_back(neighbour_index);
                    } else if (candidate > LIGHT_STRENGTH_BLANK) {
                        queue.push(neighbour_index);
                    }
                }
            }
        }

        for (size_t aperture_index : aperture_sources) {
            const LightCell& source = cells[aperture_index];
            spill_from_source(source, local_samples[aperture_index].cell, light_area_radius, cells, index_by_offset, local_samples);
        }
    }

    for (const LightEmitter& emitter : p_emitters) {
        if (emitter.z != p_z || emitter.strength <= LIGHT_STRENGTH_BLANK) {
            continue;
        }

        const Vector2i source_offset = emitter.position - p_origin;
        auto source_it = index_by_offset.find(WorldCoords::pack_coords(source_offset.x, source_offset.y));
        if (source_it == index_by_offset.end()) {
            continue;
        }

        spill_from_source(cells[source_it->second], emitter.strength, light_area_radius, cells, index_by_offset, local_samples);
    }

    for (size_t i = 0; i < cells.size(); i++) {
        r_samples[cells[i].cell_key] = local_samples[i];
    }
}

LightLevel LightMap::get_level(
    const std::unordered_map<uint64_t, LightSample>& p_samples,
    uint64_t p_cell_key
) {
    auto it = p_samples.find(p_cell_key);
    return it != p_samples.end() ? light_level_from_strength(it->second.cell) : LightLevel::Blank;
}

LightSample LightMap::get_sample(
    const std::unordered_map<uint64_t, LightSample>& p_samples,
    uint64_t p_cell_key
) {
    auto it = p_samples.find(p_cell_key);
    return it != p_samples.end() ? it->second : LightSample();
}
