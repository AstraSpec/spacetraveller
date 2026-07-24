#ifndef SPACETRAVELLER_POINT_OF_INTEREST_REGISTRY_H
#define SPACETRAVELLER_POINT_OF_INTEREST_REGISTRY_H

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3i.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace godot {

class EntityLedger;
class WorldGenerator;
struct CellArea;

struct PointOfInterestScope {
    String structure_id;
    Vector3i origin;

    bool is_valid() const { return !structure_id.is_empty(); }
    bool operator==(const PointOfInterestScope& p_other) const {
        return structure_id == p_other.structure_id && origin == p_other.origin;
    }
};

struct PointOfInterestInfo {
    Vector3i position;
    PointOfInterestScope scope;
    std::vector<String> tags;
    std::vector<String> allowed_roles;
    int dwell_min = 1;
    int dwell_max = 1;
    int weight = 1;
};

class PointOfInterestRegistry {
    std::unordered_map<uint64_t, PointOfInterestInfo> points;
    std::unordered_map<uint64_t, uint32_t> reservations;
    std::unordered_map<uint32_t, uint64_t> entity_reservations;

public:
    void clear();
    void register_for_active_cells(
        const std::vector<uint64_t>& p_active_cells,
        WorldGenerator& p_generator,
        int p_world_seed
    );
    void prune_to_area(const CellArea& p_area);

    const PointOfInterestInfo* get(const Vector3i& p_position) const;
    std::vector<Vector3i> find_compatible(
        const PointOfInterestScope& p_scope,
        int p_z,
        const Array& p_roles,
        const Vector3i& p_last_position,
        bool p_has_last_position
    ) const;
    std::vector<Vector3i> find_compatible_near(
        const Vector3i& p_origin,
        int p_radius,
        const Array& p_roles,
        const std::vector<String>& p_tags
    ) const;

    bool try_reserve(const Vector3i& p_position, uint32_t p_entity_id);
    bool is_reserved_by(const Vector3i& p_position, uint32_t p_entity_id) const;
    void release_for_entity(uint32_t p_entity_id);
    void rebuild_reservations(EntityLedger& p_ledger);
};

}

#endif // SPACETRAVELLER_POINT_OF_INTEREST_REGISTRY_H
