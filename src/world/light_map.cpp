#include "light_map.h"

#include "core/world_coords.h"
#include "data/tile_db.h"

#include <algorithm>
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

void raise_faces_for_step(LightSample& p_sample, const Vector2i& p_step, LightLevel p_level) {
    if (p_step.x > 0) {
        p_sample.raise_face(LightFace::West, p_level);
    } else if (p_step.x < 0) {
        p_sample.raise_face(LightFace::East, p_level);
    }

    if (p_step.y > 0) {
        p_sample.raise_face(LightFace::North, p_level);
    } else if (p_step.y < 0) {
        p_sample.raise_face(LightFace::South, p_level);
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

LightLevel spill_level_for_distance(int p_distance) {
    static constexpr int BRIGHT_DISTANCE = 5;
    static constexpr int LIT_DISTANCE = 9;
    static constexpr int LOW_DISTANCE = 12;

    if (p_distance <= 0) {
        return LightLevel::Bright;
    }
    if (p_distance <= BRIGHT_DISTANCE) {
        return LightLevel::Bright;
    }
    if (p_distance <= LIT_DISTANCE) {
        return LightLevel::Lit;
    }
    if (p_distance <= LOW_DISTANCE) {
        return LightLevel::Low;
    }
    return LightLevel::Blank;
}

}

void LightMap::compute_natural_light(
    const Vector2i& p_origin,
    int p_z,
    const std::vector<uint64_t>& p_offset_keys,
    const TileResolver& p_resolve_tile,
    const SkyResolver& p_is_sky_exposed,
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

    if (p_z < 0) {
        return;
    }

    std::vector<LightSample> local_samples(cells.size());
    std::vector<size_t> aperture_sources;
    std::queue<size_t> queue;

    for (size_t i = 0; i < cells.size(); i++) {
        const LightCell& cell = cells[i];
        if (!cell.passes_light || !cell.sky_exposed || cell.aperture) {
            continue;
        }
        local_samples[i].cell = LightLevel::Bright;
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
        const LightLevel current = local_samples[index].cell;
        if (current == LightLevel::Blank) {
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
            const LightLevel candidate = current;

            if (!neighbour.passes_light) {
                raise_faces_for_step(local_samples[neighbour_index], direction, candidate);
                continue;
            }

            if (static_cast<uint8_t>(candidate) > static_cast<uint8_t>(local_samples[neighbour_index].cell)) {
                local_samples[neighbour_index].cell = candidate;
                if (neighbour.aperture || !neighbour.sky_exposed) {
                    aperture_sources.push_back(neighbour_index);
                } else if (candidate != LightLevel::Blank) {
                    queue.push(neighbour_index);
                }
            }
        }
    }

    static constexpr int APERTURE_SPILL_RADIUS = 12;
    for (size_t aperture_index : aperture_sources) {
        const LightCell& source = cells[aperture_index];
        for (int oy = source.offset.y - APERTURE_SPILL_RADIUS; oy <= source.offset.y + APERTURE_SPILL_RADIUS; oy++) {
            for (int ox = source.offset.x - APERTURE_SPILL_RADIUS; ox <= source.offset.x + APERTURE_SPILL_RADIUS; ox++) {
                const int dx = ox - source.offset.x;
                const int dy = oy - source.offset.y;
                const int distance = std::max(std::abs(dx), std::abs(dy));
                if (distance > APERTURE_SPILL_RADIUS) {
                    continue;
                }

                const uint64_t offset_key = WorldCoords::pack_coords(ox, oy);
                auto target_it = index_by_offset.find(offset_key);
                if (target_it == index_by_offset.end()) {
                    continue;
                }

                const size_t target_index = target_it->second;
                Vector2i final_step;
                if (!line_has_direct_light(source.offset, cells[target_index].offset, cells, index_by_offset, &final_step)) {
                    continue;
                }

                LightLevel candidate = spill_level_for_distance(distance);
                if (cells[target_index].passes_light) {
                    local_samples[target_index].raise_cell(candidate);
                } else {
                    raise_faces_for_step(local_samples[target_index], final_step, candidate);
                }
            }
        }
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
    return it != p_samples.end() ? it->second.cell : LightLevel::Blank;
}

LightSample LightMap::get_sample(
    const std::unordered_map<uint64_t, LightSample>& p_samples,
    uint64_t p_cell_key
) {
    auto it = p_samples.find(p_cell_key);
    return it != p_samples.end() ? it->second : LightSample();
}
