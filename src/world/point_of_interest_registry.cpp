#include "point_of_interest_registry.h"

#include "cell_area.h"
#include "core/world_coords.h"
#include "data/structure_db.h"
#include "entities/entity_ledger.h"
#include "entities/entity_pool.h"
#include "world_generator.h"

#include <algorithm>

using namespace godot;

namespace {

int floor_div_chunk(int p_value) {
    return p_value >= 0
        ? p_value / WorldCoords::CHUNK_SIZE
        : (p_value - (WorldCoords::CHUNK_SIZE - 1)) / WorldCoords::CHUNK_SIZE;
}

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
                            const int chunk_x = floor_div_chunk(world_pos.x);
                            const int chunk_y = floor_div_chunk(world_pos.y);
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
                                points[packed] = std::move(info);
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
            points[packed] = std::move(info);
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
            || !roles_overlap(info.allowed_roles, p_roles)
            || reservations.find(pair.first) != reservations.end()) {
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
            || !roles_overlap(info.allowed_roles, p_roles)
            || !tags_overlap(info.tags, p_tags)
            || reservations.find(pair.first) != reservations.end()) {
            continue;
        }
        result.push_back(info.position);
    }
    return result;
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
        if (!entity
            || !profile
            || target.z != entity->z
            || !point
            || !(point->scope == scope)
            || !roles_overlap(point->allowed_roles, profile->context_tags)
            || !try_reserve(target, id)) {
            ai->routine_has_target = false;
            ai->routine_phase = RoutinePhase::SEEKING;
            ai->routine_dwell_remaining = 0;
            ai->routine_failed_attempts = 0;
        }
    }
}
