#include "point_of_interest_registry.h"

#include "cell_area.h"
#include "components/ai_controller.h"
#include "core/world_coords.h"
#include "data/structure_db.h"
#include "entities/entity_ledger.h"
#include "entities/entity_pool.h"
#include "world_generator.h"

#include <algorithm>

using namespace godot;

namespace {

Vector2i resolve_chunk_rule_local(const Vector2i& p_structure_pos, uint8_t p_rotation) {
    const int max_coord = WorldCoords::CHUNK_SIZE - 1;
    switch (p_rotation) {
        case WorldCoords::ROT_WEST:
            return Vector2i(max_coord - p_structure_pos.y, p_structure_pos.x);
        case WorldCoords::ROT_NORTH:
            return Vector2i(max_coord - p_structure_pos.x, max_coord - p_structure_pos.y);
        case WorldCoords::ROT_EAST:
            return Vector2i(p_structure_pos.y, max_coord - p_structure_pos.x);
        case WorldCoords::ROT_SOUTH:
        default:
            return p_structure_pos;
    }
}

bool roles_overlap(const std::vector<String>& p_allowed, const Array& p_roles) {
    for (const String& allowed : p_allowed) {
        for (int i = 0; i < p_roles.size(); i++) {
            if (p_roles[i].get_type() == Variant::STRING && allowed == String(p_roles[i])) {
                return true;
            }
        }
    }
    return false;
}

bool tags_overlap(const std::vector<String>& p_actual, const std::vector<String>& p_required) {
    if (p_required.empty()) return true;
    for (const String& required : p_required) {
        for (const String& actual : p_actual) {
            if (required == actual) return true;
        }
    }
    return false;
}

}

void PointOfInterestRegistry::clear() {
    points.clear();
    points_by_chunk.clear();
    reservations.clear();
    entity_reservations.clear();
}

void PointOfInterestRegistry::register_for_active_cells(
    const std::vector<uint64_t>& p_active_cells,
    WorldGenerator& p_generator,
    int p_world_seed
) {
    StructureDb* structure_db = StructureDb::get_singleton();
    if (!structure_db) return;
    auto store_point = [&](uint64_t p_key, PointOfInterestInfo&& p_info) {
        const Vector3i& position = p_info.position;
        const uint64_t chunk_key = WorldCoords::pack_coords_3d(
            WorldCoords::chunk_coord(position.x),
            WorldCoords::chunk_coord(position.y),
            position.z);
        points_by_chunk[chunk_key].insert(p_key);
        points[p_key] = std::move(p_info);
    };

    for (const uint64_t packed : p_active_cells) {
        const Vector3i world_pos = WorldCoords::unpack_coords_3d(packed);
        const StructureLevelInfo* level = nullptr;
        PointOfInterestScope scope;
        Vector2i local_pos(-1, -1);

        const DungeonStructureContext dungeon =
            p_generator.get_dungeon_structure_context(world_pos.x, world_pos.y, world_pos.z, p_world_seed);
        if (dungeon.valid) {
            const StructureInfo* structure = structure_db->get_structure_info(dungeon.structure_id);
            if (structure) {
                auto it = structure->levels.find(dungeon.local_z);
                if (it != structure->levels.end()) level = &it->second;
            }
            scope = PointOfInterestScope{dungeon.structure_id, dungeon.origin};
            local_pos = dungeon.local_pos;
        } else {
            const SurfaceFeatureContext feature =
                p_generator.get_surface_feature_context(world_pos.x, world_pos.y, world_pos.z, p_world_seed);
            if (feature.valid) {
                const StructureInfo* structure = structure_db->get_structure_info(feature.structure_id);
                if (structure) {
                    auto it = structure->levels.find(feature.local_z);
                    if (it != structure->levels.end()) level = &it->second;
                }
                scope = PointOfInterestScope{feature.structure_id, feature.origin};
                local_pos = feature.local_pos;
            } else {
                const CityStructureContext city =
                    p_generator.get_city_structure_context(world_pos.x, world_pos.y, world_pos.z);
                if (city.valid) {
                    const StructureInfo* structure = structure_db->get_structure_info(city.structure_id);
                    if (structure) {
                        auto it = structure->levels.find(city.local_z);
                        if (it != structure->levels.end()) level = &it->second;
                    }
                    scope = PointOfInterestScope{city.structure_id, city.origin};
                    local_pos = city.local_pos;
                } else {
                    const String structure_id = p_generator.get_structure_id_for_cell(
                        world_pos.x, world_pos.y, world_pos.z, p_world_seed);
                    const StructureInfo* structure = structure_id.is_empty()
                        ? nullptr
                        : structure_db->get_structure_info(structure_id);
                    if (structure) {
                        auto it = structure->levels.find(world_pos.z);
                        if (it != structure->levels.end()) {
                            level = &it->second;
                            const int chunk_x = WorldCoords::chunk_coord(world_pos.x);
                            const int chunk_y = WorldCoords::chunk_coord(world_pos.y);
                            const Vector2i chunk_origin(
                                chunk_x * WorldCoords::CHUNK_SIZE,
                                chunk_y * WorldCoords::CHUNK_SIZE
                            );
                            const Vector2i cell_local(
                                world_pos.x - chunk_origin.x,
                                world_pos.y - chunk_origin.y
                            );
                            const uint8_t rotation =
                                p_generator.get_chunk_rotation_for_cell(world_pos.x, world_pos.y);
                            scope = PointOfInterestScope{
                                structure_id,
                                Vector3i(chunk_origin.x, chunk_origin.y, 0)
                            };
                            for (const StructureRuleInfo& rule : level->rules) {
                                if (rule.type != RuleType::POINT_OF_INTEREST
                                    || resolve_chunk_rule_local(rule.pos, rotation) != cell_local) {
                                    continue;
                                }
                                PointOfInterestInfo info;
                                info.position = world_pos;
                                info.scope = scope;
                                info.tags = rule.poi_tags;
                                info.allowed_roles = rule.poi_allowed_roles;
                                info.dwell_min = rule.poi_dwell_min;
                                info.dwell_max = rule.poi_dwell_max;
                                info.weight = rule.poi_weight;
                                info.initial_visitor_spawn = rule.poi_initial_visitor_spawn;
                                store_point(packed, std::move(info));
                            }
                            continue;
                        }
                    }
                }
            }
        }

        if (!level || !scope.is_valid()) continue;
        for (const StructureRuleInfo& rule : level->rules) {
            if (rule.type != RuleType::POINT_OF_INTEREST || rule.pos != local_pos) continue;
            PointOfInterestInfo info;
            info.position = world_pos;
            info.scope = scope;
            info.tags = rule.poi_tags;
            info.allowed_roles = rule.poi_allowed_roles;
            info.dwell_min = rule.poi_dwell_min;
            info.dwell_max = rule.poi_dwell_max;
            info.weight = rule.poi_weight;
            info.initial_visitor_spawn = rule.poi_initial_visitor_spawn;
            store_point(packed, std::move(info));
        }
    }
}

void PointOfInterestRegistry::prune_to_area(const CellArea& p_area) {
    std::vector<uint64_t> removed;
    for (const auto& pair : points) {
        const Vector3i pos = pair.second.position;
        if (!p_area.contains_world(pos.x, pos.y, pos.z)) {
            removed.push_back(pair.first);
        }
    }
    for (const uint64_t key : removed) {
        auto point = points.find(key);
        if (point != points.end()) {
            const Vector3i& position = point->second.position;
            const uint64_t chunk_key = WorldCoords::pack_coords_3d(
                WorldCoords::chunk_coord(position.x),
                WorldCoords::chunk_coord(position.y),
                position.z);
            auto bucket = points_by_chunk.find(chunk_key);
            if (bucket != points_by_chunk.end()) {
                bucket->second.erase(key);
                if (bucket->second.empty()) points_by_chunk.erase(bucket);
            }
        }
        auto reservation = reservations.find(key);
        if (reservation != reservations.end()) {
            entity_reservations.erase(reservation->second);
            reservations.erase(reservation);
        }
        points.erase(key);
    }
}

const PointOfInterestInfo* PointOfInterestRegistry::get(const Vector3i& p_position) const {
    auto it = points.find(WorldCoords::pack_coords_3d(p_position.x, p_position.y, p_position.z));
    return it == points.end() ? nullptr : &it->second;
}

bool PointOfInterestRegistry::is_available_to_roles(
    uint64_t p_point_key,
    const PointOfInterestInfo& p_info,
    const Array& p_roles
) const {
    return roles_overlap(p_info.allowed_roles, p_roles)
        && reservations.find(p_point_key) == reservations.end();
}

std::vector<Vector3i> PointOfInterestRegistry::find_compatible(
    const PointOfInterestScope& p_scope,
    int p_z,
    const Array& p_roles,
    const Vector3i& p_last_position,
    bool p_has_last_position
) const {
    std::vector<Vector3i> preferred;
    std::vector<Vector3i> repeated;
    for (const auto& pair : points) {
        const PointOfInterestInfo& info = pair.second;
        if (info.position.z != p_z || !(info.scope == p_scope)
            || !is_available_to_roles(pair.first, info, p_roles)) {
            continue;
        }
        if (p_has_last_position && info.position == p_last_position) {
            repeated.push_back(info.position);
        } else {
            preferred.push_back(info.position);
        }
    }
    return preferred.empty() ? repeated : preferred;
}

std::vector<Vector3i> PointOfInterestRegistry::find_compatible_near(
    const Vector3i& p_origin,
    int p_radius,
    const Array& p_roles,
    const std::vector<String>& p_tags
) const {
    std::vector<Vector3i> result;
    const int radius = std::max(0, p_radius);
    for (const auto& pair : points) {
        const PointOfInterestInfo& info = pair.second;
        if (info.position.z != p_origin.z
            || std::max(
                std::abs(info.position.x - p_origin.x),
                std::abs(info.position.y - p_origin.y)) > radius
            || !tags_overlap(info.tags, p_tags)
            || !is_available_to_roles(pair.first, info, p_roles)) {
            continue;
        }
        result.push_back(info.position);
    }
    return result;
}

std::vector<Vector3i> PointOfInterestRegistry::find_compatible_in_chunk(
    const Vector3i& p_chunk,
    const Array& p_roles,
    const Vector3i& p_last_position,
    bool p_has_last_position
) const {
    std::vector<Vector3i> preferred;
    const uint64_t chunk_key =
        WorldCoords::pack_coords_3d(p_chunk.x, p_chunk.y, p_chunk.z);
    auto bucket = points_by_chunk.find(chunk_key);
    if (bucket == points_by_chunk.end()) return preferred;
    for (const uint64_t point_key : bucket->second) {
        auto point = points.find(point_key);
        if (point == points.end()) continue;
        const PointOfInterestInfo& info = point->second;
        if (!is_available_to_roles(point_key, info, p_roles)) {
            continue;
        }
        if (p_has_last_position && info.position == p_last_position) {
            continue;
        } else {
            preferred.push_back(info.position);
        }
    }
    return preferred;
}

std::vector<Vector3i> PointOfInterestRegistry::find_in_chunk(
    const Vector3i& p_chunk
) const {
    std::vector<Vector3i> result;
    const uint64_t chunk_key =
        WorldCoords::pack_coords_3d(p_chunk.x, p_chunk.y, p_chunk.z);
    auto bucket = points_by_chunk.find(chunk_key);
    if (bucket == points_by_chunk.end()) return result;
    result.reserve(bucket->second.size());
    for (const uint64_t point_key : bucket->second) {
        auto point = points.find(point_key);
        if (point != points.end()) result.push_back(point->second.position);
    }
    std::sort(result.begin(), result.end(), [](const Vector3i& a, const Vector3i& b) {
        if (a.y != b.y) return a.y < b.y;
        return a.x < b.x;
    });
    return result;
}

bool PointOfInterestRegistry::is_reserved(const Vector3i& p_position) const {
    const uint64_t key =
        WorldCoords::pack_coords_3d(p_position.x, p_position.y, p_position.z);
    return reservations.find(key) != reservations.end();
}

bool PointOfInterestRegistry::try_reserve_weighted(
    std::vector<Vector3i> p_candidates,
    uint32_t p_entity_id,
    double p_normalized_roll,
    Vector3i& r_selected
) {
    p_candidates.erase(
        std::remove_if(
            p_candidates.begin(),
            p_candidates.end(),
            [&](const Vector3i& candidate) {
                return get(candidate) == nullptr;
            }),
        p_candidates.end());
    if (p_candidates.empty()) return false;
    std::sort(
        p_candidates.begin(),
        p_candidates.end(),
        [](const Vector3i& a, const Vector3i& b) {
            return WorldCoords::pack_coords_3d(a.x, a.y, a.z)
                < WorldCoords::pack_coords_3d(b.x, b.y, b.z);
        });

    int total_weight = 0;
    for (const Vector3i& candidate : p_candidates) {
        const PointOfInterestInfo* point = get(candidate);
        if (point) total_weight += std::max(1, point->weight);
    }
    if (total_weight <= 0) return false;

    const double normalized_roll =
        std::clamp(p_normalized_roll, 0.0, 0.9999999999999999);
    int weighted_roll = static_cast<int>(
        normalized_roll * static_cast<double>(total_weight));
    size_t selected_index = p_candidates.size() - 1;
    for (size_t index = 0; index < p_candidates.size(); ++index) {
        const PointOfInterestInfo* point = get(p_candidates[index]);
        if (!point) continue;
        weighted_roll -= std::max(1, point->weight);
        if (weighted_roll < 0) {
            selected_index = index;
            break;
        }
    }

    for (size_t offset = 0; offset < p_candidates.size(); ++offset) {
        const size_t index = (selected_index + offset) % p_candidates.size();
        if (try_reserve(p_candidates[index], p_entity_id)) {
            r_selected = p_candidates[index];
            return true;
        }
    }
    return false;
}

bool PointOfInterestRegistry::try_reserve(const Vector3i& p_position, uint32_t p_entity_id) {
    const uint64_t key = WorldCoords::pack_coords_3d(p_position.x, p_position.y, p_position.z);
    if (points.find(key) == points.end()) return false;
    auto occupied = reservations.find(key);
    if (occupied != reservations.end() && occupied->second != p_entity_id) return false;

    auto previous = entity_reservations.find(p_entity_id);
    if (previous != entity_reservations.end() && previous->second != key) {
        reservations.erase(previous->second);
    }
    reservations[key] = p_entity_id;
    entity_reservations[p_entity_id] = key;
    return true;
}

bool PointOfInterestRegistry::is_reserved_by(
    const Vector3i& p_position,
    uint32_t p_entity_id
) const {
    const uint64_t key = WorldCoords::pack_coords_3d(p_position.x, p_position.y, p_position.z);
    auto it = reservations.find(key);
    return it != reservations.end() && it->second == p_entity_id;
}

void PointOfInterestRegistry::release_for_entity(uint32_t p_entity_id) {
    auto it = entity_reservations.find(p_entity_id);
    if (it == entity_reservations.end()) return;
    reservations.erase(it->second);
    entity_reservations.erase(it);
}

void PointOfInterestRegistry::rebuild_reservations(EntityLedger& p_ledger) {
    std::vector<uint32_t> stale_reservations;
    for (const auto& pair : entity_reservations) {
        if (!p_ledger.get_entity_pool().get_entity(pair.first)) {
            stale_reservations.push_back(pair.first);
        }
    }
    for (const uint32_t id : stale_reservations) {
        release_for_entity(id);
    }

    std::vector<uint32_t> ids = p_ledger.get_entity_pool().get_live_ids();
    std::sort(ids.begin(), ids.end());
    for (const uint32_t id : ids) {
        AIData* ai = p_ledger.try_get_ai(id);
        if (!ai || !ai->routine_has_target) continue;
        const Entity* entity = p_ledger.get_entity_pool().get_entity(id);
        const SocialProfileData* profile = p_ledger.try_get_social_profile(id);
        const Vector3i target = ai->routine_target;
        const PointOfInterestInfo* point = get(target);
        const PointOfInterestScope scope{ai->routine_structure_id, ai->routine_scope_origin};
        const bool target_matches_routine = point
            && (ai->has_routine_scope
                ? point->scope == scope
                : p_ledger.is_ambient(id));
        if (!entity
            || !profile
            || target.z != entity->z
            || !target_matches_routine
            || !roles_overlap(point->allowed_roles, profile->context_tags)
            || !try_reserve(target, id)) {
            release_for_entity(id);
            AIController::reset_routine(*ai, false);
        }
    }
}
