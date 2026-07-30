#include "race_db.h"
#include "core/id_registry.h"
#include "core/tag_registry.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

template<> RaceDb* DataBase<RaceInfo, RaceDb>::singleton = nullptr;

void RaceDb::_bind_methods() {
    ClassDB::bind_static_method("RaceDb", D_METHOD("get_singleton"), &RaceDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &RaceDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_ids"), &RaceDb::get_ids);
    ClassDB::bind_method(D_METHOD("has_tag", "id", "tag"), &RaceDb::has_tag);
    ClassDB::bind_method(D_METHOD("get_atlas_coords", "id"), static_cast<Vector2i (RaceDb::*)(const String&) const>(&RaceDb::get_atlas_coords));
}

RaceDb::RaceDb() {}
RaceDb::~RaceDb() {}

RaceInfo RaceDb::_parse_row(const Dictionary &p_data) {
    RaceInfo info;
    info.name = p_data.get("name", "");
    info.atlas = variant_to_vector2i(p_data.get("atlas", Array()));
    info.downed_atlas_offset =
        static_cast<int>(p_data.get("downed_atlas_offset", -1));
    info.anatomy_scale = MAX(0.01f, static_cast<float>(static_cast<double>(
        p_data.get("anatomy_scale", 1.0))));
    info.speed = static_cast<float>(static_cast<double>(p_data.get("speed", 1.0)));
    info.base_damage = static_cast<float>(static_cast<double>(p_data.get("base_damage", 10.0)));
    info.base_stamina = static_cast<float>(static_cast<double>(p_data.get("base_stamina", 100.0)));
    info.corpse_item = p_data.get("corpse_item", "");
    String death_loot_table = String(p_data.get("death_loot_table", ""));
    if (!death_loot_table.is_empty() && IdRegistry::get_singleton()) {
        info.death_loot_table = IdRegistry::get_singleton()->register_string(death_loot_table);
    }
    info.combat_style = p_data.get("combat_style", "default");
    info.faction = String(p_data.get("faction", "unaffiliated")).to_lower();
    info.reaction_policy = String(p_data.get("reaction_policy", "defensive")).to_lower();
    Dictionary attack_policy =
        p_data.get("attack_policy", Dictionary());
    info.downed_target_policy = String(
        attack_policy.get("downed_targets", "continue")).to_lower();
    info.reaction_radius = static_cast<int>(p_data.get("reaction_radius", 12));
    info.traversal_profile = String(p_data.get("traversal_profile", "walker")).to_lower();
    info.tags = _parse_tags(p_data.get("tags", Array()));
    info.light = parse_light_emission(p_data.get("light", Variant()));

    if (p_data.has("natural_armor")) {
        Dictionary armor = p_data["natural_armor"];
        info.natural_armor.enabled = true;
        info.natural_armor.coverage = CLAMP(
            static_cast<float>(static_cast<double>(armor.get("coverage", 0.0))),
            0.0f, 1.0f);
        info.natural_armor.bash = MAX(
            0.0f, static_cast<float>(static_cast<double>(armor.get("bash", 0.0))));
        info.natural_armor.cut = MAX(
            0.0f, static_cast<float>(static_cast<double>(armor.get("cut", 0.0))));
        info.natural_armor.pierce = MAX(
            0.0f, static_cast<float>(static_cast<double>(armor.get("pierce", 0.0))));
        info.natural_armor.bash_transmission = CLAMP(
            static_cast<float>(static_cast<double>(
                armor.get("bash_transmission", 1.0))),
            0.0f, 1.0f);
    }

    Array parts = p_data.get("parts", Array());
    for (int i = 0; i < parts.size(); i++) {
        Dictionary p = parts[i];
        RacePartDefinition def;
        def.part_id = p.get("id", "");
        def.parent_part_id = p.get("parent", "");
        def.count = p.get("count", 1);
        def.height = p.get("height", "MID");
        def.integrity_scale = MAX(0.01f, static_cast<float>(static_cast<double>(
            p.get("integrity_scale", 1.0))));
        info.parts.push_back(def);
    }

    if (IdRegistry::get_singleton()) {
        uint16_t id = IdRegistry::get_singleton()->register_string(p_data["id"]);
        if (id >= fast_cache.size()) {
            fast_cache.resize(id + 1);
        }
        fast_cache[id] = info;
    }

    return info;
}

const RaceInfo* RaceDb::get_race_info(const String &p_id) const {
    return get_info(p_id);
}

const RaceInfo* RaceDb::get_race_info(uint16_t p_id) const {
    if (p_id < fast_cache.size()) {
        return &fast_cache[p_id];
    }
    return nullptr;
}

bool RaceDb::has_tag(const String &p_id, const String &p_tag) const {
    const RaceInfo* info = get_race_info(p_id);
    if (!info) return false;

    TagRegistry *reg = TagRegistry::get_singleton();
    if (!reg) return false;

    uint16_t tag_id = reg->get_tag_id(p_tag);
    return TagRegistry::has_tag(tag_id, info->tags);
}

Vector2i RaceDb::get_atlas_coords(const String &p_id) const {
    const RaceInfo* info = get_race_info(p_id);
    if (info) return info->atlas;
    return Vector2i(-1, -1);
}

Vector2i RaceDb::get_atlas_coords(uint16_t p_id) const {
    const RaceInfo* info = get_race_info(p_id);
    if (info) return info->atlas;
    return Vector2i(-1, -1);
}

}
