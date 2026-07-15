#ifndef SPACETRAVELLER_ORE_DB_H
#define SPACETRAVELLER_ORE_DB_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/string.hpp>
#include <cstdint>
#include <vector>

#include "database.h"

namespace godot {

struct OreIntRange {
    int min = 0;
    int max = 0;
};

struct OreFloatRange {
    float min = 0.0f;
    float max = 0.0f;
};

struct OreMineralInfo {
    String tile;
    uint16_t tile_id = 0;
    int weight = 1;
    int cumulative_weight = 0;
};

struct OreFormationInfo {
    String id;
    String type;
    int candidate_slots = 1;

    String province_key;
    int province_scale_chunks = 12;
    float province_threshold = 0.5f;
    float outside_chance = 0.0f;
    float inside_chance = 0.0f;

    OreIntRange depth;
    OreIntRange thickness;
    OreIntRange length;
    OreIntRange width;
    OreIntRange cells;
    OreFloatRange richness;

    std::vector<OreMineralInfo> minerals;
    int total_mineral_weight = 0;
};

class OreDb : public Object, public DataBase<OreFormationInfo, OreDb> {
    GDCLASS(OreDb, Object)

protected:
    static void _bind_methods();
    virtual OreFormationInfo _parse_row(const Dictionary& p_data) override;

private:
    std::vector<OreFormationInfo> valid_formations;
    bool validate_formation(OreFormationInfo& r_info) const;

public:
    OreDb() = default;
    ~OreDb() = default;

    void initialize_data();
    Array get_ids() const;
    const std::vector<OreFormationInfo>& get_formations() const { return valid_formations; }
    const OreFormationInfo* get_formation(const String& p_id) const;
};

}

#endif // SPACETRAVELLER_ORE_DB_H
