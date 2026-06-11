#include "entity_tracker.h"

#include "core/world_coords.h"
#include "entities/entity.h"
#include "entities/entity_pool.h"

#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>
#include <cmath>

using namespace godot;

namespace {

static int floor_div(int value, int divisor) {
    return value >= 0 ? value / divisor : (value - (divisor - 1)) / divisor;
}

}

uint64_t EntityTracker::cell_key(const Vector2i& pos) {
    return WorldCoords::pack_coords(pos.x, pos.y);
}

Vector2i EntityTracker::bucket_pos(const Vector2i& pos) {
    return Vector2i(
        floor_div(pos.x, WorldCoords::CHUNK_SIZE),
        floor_div(pos.y, WorldCoords::CHUNK_SIZE)
    );
}

uint64_t EntityTracker::bucket_key(const Vector2i& pos) {
    Vector2i bucket = bucket_pos(pos);
    return WorldCoords::pack_coords(bucket.x, bucket.y);
}

void EntityTracker::add_to_bucket(uint32_t entity_id, const Vector2i& pos) {
    entities_by_bucket[bucket_key(pos)].insert(entity_id);
}

void EntityTracker::remove_from_bucket(uint32_t entity_id, const Vector2i& pos) {
    uint64_t key = bucket_key(pos);
    auto it = entities_by_bucket.find(key);
    if (it == entities_by_bucket.end()) return;

    it->second.erase(entity_id);
    if (it->second.empty()) {
        entities_by_bucket.erase(it);
    }
}

bool EntityTracker::insert(uint32_t entity_id, const Vector2i& pos) {
    uint64_t key = cell_key(pos);
    auto cell_it = entities_by_cell.find(key);
    if (cell_it != entities_by_cell.end() && cell_it->second != entity_id) {
        UtilityFunctions::printerr("Cell already occupied");
        return false;
    }

    auto existing = positions_by_entity.find(entity_id);
    if (existing != positions_by_entity.end()) {
        if (existing->second == pos) return true;
        remove(entity_id);
    }

    positions_by_entity[entity_id] = pos;
    entities_by_cell[key] = entity_id;
    add_to_bucket(entity_id, pos);
    return true;
}

bool EntityTracker::move(uint32_t entity_id, const Vector2i& old_pos, const Vector2i& new_pos) {
    if (old_pos == new_pos) return contains(entity_id) || insert(entity_id, new_pos);

    auto existing = positions_by_entity.find(entity_id);
    if (existing == positions_by_entity.end()) {
        return insert(entity_id, new_pos);
    }

    uint64_t new_key = cell_key(new_pos);
    auto cell_it = entities_by_cell.find(new_key);
    if (cell_it != entities_by_cell.end() && cell_it->second != entity_id) {
        UtilityFunctions::printerr("Cell already occupied");
        return false;
    }

    const Vector2i current_pos = existing->second;
    uint64_t old_key = cell_key(old_pos);
    auto old_cell_it = entities_by_cell.find(old_key);
    if (old_cell_it != entities_by_cell.end() && old_cell_it->second == entity_id) {
        entities_by_cell.erase(old_cell_it);
    } else {
        entities_by_cell.erase(cell_key(current_pos));
    }

    remove_from_bucket(entity_id, current_pos);
    positions_by_entity[entity_id] = new_pos;
    entities_by_cell[new_key] = entity_id;
    add_to_bucket(entity_id, new_pos);
    return true;
}

void EntityTracker::remove(uint32_t entity_id) {
    auto it = positions_by_entity.find(entity_id);
    if (it == positions_by_entity.end()) return;

    Vector2i pos = it->second;
    positions_by_entity.erase(it);

    uint64_t key = cell_key(pos);
    auto cell_it = entities_by_cell.find(key);
    if (cell_it != entities_by_cell.end() && cell_it->second == entity_id) {
        entities_by_cell.erase(cell_it);
    }
    remove_from_bucket(entity_id, pos);
}

void EntityTracker::clear() {
    positions_by_entity.clear();
    entities_by_cell.clear();
    entities_by_bucket.clear();
}

bool EntityTracker::contains(uint32_t entity_id) const {
    return positions_by_entity.find(entity_id) != positions_by_entity.end();
}

bool EntityTracker::get_position(uint32_t entity_id, Vector2i& out_pos) const {
    auto it = positions_by_entity.find(entity_id);
    if (it == positions_by_entity.end()) return false;
    out_pos = it->second;
    return true;
}

uint32_t EntityTracker::get_at(const Vector2i& pos) const {
    auto it = entities_by_cell.find(cell_key(pos));
    return it == entities_by_cell.end() ? INVALID_ENTITY_ID : it->second;
}

void EntityTracker::query_rect(const Vector2i& min_pos, const Vector2i& max_pos, std::vector<uint32_t>& out_ids) const {
    Vector2i min_bucket = bucket_pos(min_pos);
    Vector2i max_bucket = bucket_pos(max_pos);

    for (int by = min_bucket.y; by <= max_bucket.y; by++) {
        for (int bx = min_bucket.x; bx <= max_bucket.x; bx++) {
            auto bucket_it = entities_by_bucket.find(WorldCoords::pack_coords(bx, by));
            if (bucket_it == entities_by_bucket.end()) continue;

            for (uint32_t entity_id : bucket_it->second) {
                auto pos_it = positions_by_entity.find(entity_id);
                if (pos_it == positions_by_entity.end()) continue;
                const Vector2i& pos = pos_it->second;
                if (pos.x < min_pos.x || pos.x > max_pos.x || pos.y < min_pos.y || pos.y > max_pos.y) {
                    continue;
                }
                out_ids.push_back(entity_id);
            }
        }
    }
}

void EntityTracker::query_radius(const Vector2i& center, int radius, std::vector<uint32_t>& out_ids) const {
    if (radius < 0) return;

    std::vector<uint32_t> rect_ids;
    rect_ids.reserve(out_ids.size());
    query_rect(
        Vector2i(center.x - radius, center.y - radius),
        Vector2i(center.x + radius, center.y + radius),
        rect_ids
    );

    const long radius_sq = static_cast<long>(radius) * radius;
    for (uint32_t entity_id : rect_ids) {
        auto pos_it = positions_by_entity.find(entity_id);
        if (pos_it == positions_by_entity.end()) continue;

        const long dx = static_cast<long>(pos_it->second.x - center.x);
        const long dy = static_cast<long>(pos_it->second.y - center.y);
        if (dx * dx + dy * dy <= radius_sq) {
            out_ids.push_back(entity_id);
        }
    }
}

void EntityTracker::collect_ids(std::vector<uint32_t>& out_ids) const {
    out_ids.reserve(out_ids.size() + positions_by_entity.size());
    for (const auto& [entity_id, pos] : positions_by_entity) {
        out_ids.push_back(entity_id);
    }
}

void EntityTracker::rebuild_from_pool(const EntityPool& pool) {
    clear();
    for (uint32_t entity_id : pool.get_live_ids()) {
        const Entity* entity = pool.get_entity(entity_id);
        if (!entity) continue;
        insert(entity_id, Vector2i(entity->x, entity->y));
    }
}
