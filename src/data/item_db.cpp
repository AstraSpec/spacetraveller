#include "item_db.h"
#include "core/id_registry.h"
#include "item_category_db.h"
#include "weapon_profile_db.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

template<> ItemDb* DataBase<ItemInfo, ItemDb>::singleton = nullptr;

namespace {

uint8_t capability_bit(ItemCapability p_capability) {
    return static_cast<uint8_t>(p_capability);
}

bool item_has_capability(
    const ItemInfo& p_info,
    ItemCapability p_capability
) {
    return (p_info.capabilities & capability_bit(p_capability)) != 0;
}

ItemCapability capability_from_string(const String& p_capability) {
    const String capability = p_capability.to_lower();
    if (capability == "weapon") return ItemCapability::WEAPON;
    if (capability == "clothing") return ItemCapability::CLOTHING;
    if (capability == "light") return ItemCapability::LIGHT;
    return ItemCapability::NONE;
}

bool is_numeric(const Variant& p_value) {
    return p_value.get_type() == Variant::INT ||
        p_value.get_type() == Variant::FLOAT;
}

bool is_integral_number(const Variant& p_value) {
    if (!is_numeric(p_value)) return false;
    const double value = static_cast<double>(p_value);
    return std::isfinite(value) && std::floor(value) == value;
}

bool validate_number_range(
    const Dictionary& p_data,
    const String& p_field,
    double p_minimum,
    double p_maximum,
    String& r_error
) {
    if (!p_data.has(p_field) || !is_numeric(p_data[p_field])) {
        r_error = "must be numeric";
        return false;
    }
    const double value = static_cast<double>(p_data[p_field]);
    if (value < p_minimum || value > p_maximum) {
        r_error = "is outside its supported range";
        return false;
    }
    return true;
}

Dictionary clothing_slot_to_dictionary(const ClothingSlotInfo& slot) {
    Dictionary d;
    d["part"] = slot.part;
    d["layer"] = slot.layer;
    d["coverage"] = slot.coverage;
    d["bash"] = slot.bash;
    d["cut"] = slot.cut;
    d["pierce"] = slot.pierce;
    d["bash_transmission"] = slot.bash_transmission;
    return d;
}

bool parse_clothing_slot(const Dictionary& p_data, ClothingSlotInfo& r_slot) {
    String part = String(p_data.get("part", "")).strip_edges();
    if (part.is_empty()) return false;

    r_slot.part = part;
    r_slot.layer = String(p_data.get("layer", "middle")).strip_edges();
    if (r_slot.layer.is_empty()) r_slot.layer = "middle";

    r_slot.coverage = std::clamp(
        static_cast<float>(static_cast<double>(p_data.get("coverage", 1.0))),
        0.0f,
        1.0f
    );
    r_slot.bash = std::max(
        0.0f,
        static_cast<float>(static_cast<double>(p_data.get("bash", 0.0)))
    );
    r_slot.cut = std::max(
        0.0f,
        static_cast<float>(static_cast<double>(p_data.get("cut", 0.0)))
    );
    r_slot.pierce = std::max(
        0.0f,
        static_cast<float>(static_cast<double>(p_data.get("pierce", 0.0)))
    );
    r_slot.bash_transmission = std::clamp(
        static_cast<float>(static_cast<double>(p_data.get("bash_transmission", 0.0))),
        0.0f,
        1.0f
    );
    return true;
}

std::vector<ClothingSlotInfo> parse_clothing_slots(const Variant& p_clothing) {
    std::vector<ClothingSlotInfo> slots;
    if (p_clothing.get_type() == Variant::ARRAY) {
        Array entries = p_clothing;
        for (int i = 0; i < entries.size(); i++) {
            Variant entry = entries[i];
            if (entry.get_type() != Variant::DICTIONARY) continue;

            ClothingSlotInfo slot;
            if (parse_clothing_slot(entry, slot)) {
                slots.push_back(slot);
            }
        }
    }
    return slots;
}

}

void ItemDb::_bind_methods() {
    ClassDB::bind_static_method("ItemDb", D_METHOD("get_singleton"), &ItemDb::get_singleton);
    ClassDB::bind_method(D_METHOD("initialize_data"), &ItemDb::initialize_data);
    ClassDB::bind_method(D_METHOD("get_atlas_coords", "id"), &ItemDb::get_atlas_coords);
    ClassDB::bind_method(D_METHOD("get_item_name", "id"), &ItemDb::get_item_name);
    ClassDB::bind_method(D_METHOD("get_item_description", "id"), &ItemDb::get_item_description);
    ClassDB::bind_method(D_METHOD("get_item_price", "id"), &ItemDb::get_item_price);
    ClassDB::bind_method(D_METHOD("get_item_modifiers", "id"), &ItemDb::get_item_modifiers);
    ClassDB::bind_method(D_METHOD("has_tag", "id", "tag"), &ItemDb::has_tag);
    ClassDB::bind_method(
        D_METHOD("has_capability", "id", "capability"),
        &ItemDb::has_capability);
    ClassDB::bind_method(
        D_METHOD("get_capabilities", "id"),
        &ItemDb::get_capabilities);
    ClassDB::bind_method(D_METHOD("get_clothing_data", "id"), &ItemDb::get_clothing_data);
    ClassDB::bind_method(D_METHOD("get_clothing_slots", "id"), &ItemDb::get_clothing_slots);
    ClassDB::bind_method(D_METHOD("get_weapon_data", "id"), &ItemDb::get_weapon_data);
    ClassDB::bind_method(
        D_METHOD("get_item_category", "id"),
        &ItemDb::get_item_category);
    ClassDB::bind_method(D_METHOD("get_ids"), &ItemDb::get_ids);
}

ItemDb::ItemDb() {
}

ItemDb::~ItemDb() {
}

bool ItemDb::_validate_row(
    const Dictionary& p_data,
    String& r_error
) const {
    if (p_data.has("type")) {
        r_error = "field 'type' is unsupported; use presentation field 'category'";
        return false;
    }

    if (!p_data.has("category") ||
        p_data["category"].get_type() != Variant::STRING) {
        r_error = "field 'category' must be a non-empty string";
        return false;
    }
    const String category = String(p_data["category"]).strip_edges();
    ItemCategoryDb* categories = ItemCategoryDb::get_singleton();
    if (category.is_empty()) {
        r_error = "field 'category' must be a non-empty string";
        return false;
    }
    if (!categories || !categories->get_category_info(category)) {
        r_error = "field 'category' references unknown category '" +
            category + "'";
        return false;
    }

    if (p_data.has("weapon")) {
        if (p_data["weapon"].get_type() != Variant::DICTIONARY) {
            r_error = "capability 'weapon' must be an object";
            return false;
        }
        const Dictionary weapon = p_data["weapon"];
        if (!weapon.has("attack_profile") ||
            weapon["attack_profile"].get_type() != Variant::STRING) {
            r_error =
                "capability 'weapon.attack_profile' must be a non-empty string";
            return false;
        }
        const String profile =
            String(weapon["attack_profile"]).strip_edges();
        WeaponProfileDb* profiles = WeaponProfileDb::get_singleton();
        if (profile.is_empty()) {
            r_error =
                "capability 'weapon.attack_profile' must be a non-empty string";
            return false;
        }
        if (!profiles || !profiles->get_profile_info(profile)) {
            r_error = "capability 'weapon.attack_profile' references unknown "
                "profile '" + profile + "'";
            return false;
        }
        if (!validate_number_range(
                weapon, "damage", 0.0, 1000000.0, r_error)) {
            r_error = "capability 'weapon.damage' " + r_error;
            return false;
        }
        String load_error;
        if (!validate_number_range(
                weapon,
                "manipulation_load",
                0.01,
                1000000.0,
                load_error)) {
            r_error = "capability 'weapon.manipulation_load' " +
                load_error;
            return false;
        }
        if (!weapon.has("reach") ||
            !is_integral_number(weapon["reach"]) ||
            static_cast<int>(weapon["reach"]) < 1) {
            r_error = "capability 'weapon.reach' must be an integer of at least 1";
            return false;
        }
    }

    if (p_data.has("clothing")) {
        if (p_data["clothing"].get_type() != Variant::ARRAY) {
            r_error = "capability 'clothing' must be an array";
            return false;
        }
        const Array slots = p_data["clothing"];
        if (slots.is_empty()) {
            r_error = "capability 'clothing' must contain at least one slot";
            return false;
        }
        for (int i = 0; i < slots.size(); ++i) {
            if (slots[i].get_type() != Variant::DICTIONARY) {
                r_error = "capability 'clothing[" + String::num_int64(i) +
                    "]' must be an object";
                return false;
            }
            const Dictionary slot = slots[i];
            if (!slot.has("part") ||
                slot["part"].get_type() != Variant::STRING ||
                String(slot["part"]).strip_edges().is_empty()) {
                r_error = "capability 'clothing[" + String::num_int64(i) +
                    "].part' must be a non-empty string";
                return false;
            }
            if (!slot.has("layer") ||
                slot["layer"].get_type() != Variant::STRING) {
                r_error = "capability 'clothing[" + String::num_int64(i) +
                    "].layer' is invalid";
                return false;
            }
            const String layer = String(slot["layer"]).strip_edges();
            if (layer != "outer" && layer != "armor" &&
                layer != "middle" && layer != "under") {
                r_error = "capability 'clothing[" + String::num_int64(i) +
                    "].layer' is invalid";
                return false;
            }
            const String fields[] = {
                "coverage", "bash", "cut", "pierce", "bash_transmission"
            };
            for (const String& field : fields) {
                const double maximum =
                    field == "coverage" || field == "bash_transmission"
                    ? 1.0
                    : 1000000.0;
                String field_error;
                if (!validate_number_range(
                        slot, field, 0.0, maximum, field_error)) {
                    r_error = "capability 'clothing[" +
                        String::num_int64(i) + "]." + field + "' " +
                        field_error;
                    return false;
                }
            }
        }
    }

    if (p_data.has("light")) {
        if (p_data["light"].get_type() != Variant::DICTIONARY) {
            r_error = "capability 'light' must be an object";
            return false;
        }
        const Dictionary light = p_data["light"];
        if (!light.has("strength") ||
            !is_integral_number(light["strength"]) ||
            static_cast<int>(light["strength"]) <= 0 ||
            static_cast<int>(light["strength"]) >
                std::numeric_limits<uint16_t>::max()) {
            r_error = "capability 'light.strength' must be an integer from 1 to 65535";
            return false;
        }
    }
    return true;
}

ItemInfo ItemDb::_parse_row(const Dictionary &p_data) {
    ItemInfo info;
    info.name = p_data.get("name", "");
    info.description = p_data.get("description", "");
    info.atlas = variant_to_vector2i(p_data.get("atlas", Array()));
    info.weight = p_data.get("weight", 0.0f);
    info.price = int(p_data.get("price", 0));
    if (info.price < 0) info.price = 0;
    info.tags = _parse_tags(p_data.get("tags", Array()));
    info.category_id = String(p_data["category"]).strip_edges();

    if (p_data.has("clothing")) {
        info.clothing_slots = parse_clothing_slots(p_data["clothing"]);
        info.capabilities |= capability_bit(ItemCapability::CLOTHING);
    }
    if (p_data.has("weapon")) {
        const Dictionary weapon = p_data["weapon"];
        info.weapon.attack_profile =
            String(weapon["attack_profile"]).strip_edges();
        info.weapon.damage =
            static_cast<float>(static_cast<double>(weapon["damage"]));
        info.weapon.manipulation_load =
            static_cast<float>(
                static_cast<double>(weapon["manipulation_load"]));
        info.weapon.reach = static_cast<int>(weapon["reach"]);
        info.capabilities |= capability_bit(ItemCapability::WEAPON);
    }
    if (p_data.has("light")) {
        info.light = parse_light_emission(p_data["light"]);
        info.capabilities |= capability_bit(ItemCapability::LIGHT);
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

const ItemInfo* ItemDb::get_item_info(const String &p_id) const {
    return get_info(p_id);
}

const ItemInfo* ItemDb::get_item_info(uint16_t p_id) const {
    if (p_id < fast_cache.size()) {
        return &fast_cache[p_id];
    }
    return nullptr;
}

Vector2i ItemDb::get_atlas_coords(const String &p_id) const {
    const ItemInfo* info = get_item_info(p_id);
    if (info) return info->atlas;
    return Vector2i(-1, -1);
}

String ItemDb::get_item_name(const String &p_id) const {
    const ItemInfo* info = get_item_info(p_id);
    if (info) return info->name;
    return "";
}

String ItemDb::get_item_description(const String &p_id) const {
    const ItemInfo* info = get_item_info(p_id);
    if (info) return info->description;
    return "";
}

int ItemDb::get_item_price(const String &p_id) const {
    const ItemInfo* info = get_item_info(p_id);
    return info ? info->price : 0;
}

Dictionary ItemDb::get_item_modifiers(const String &p_id) const {
    const ItemInfo* info = get_item_info(p_id);
    Dictionary d;
    if (info) {
        if (info->weight > 0.0f) d["weight"] = info->weight;
        if (info->price > 0) d["price"] = info->price;
    }
    return d;
}

bool ItemDb::has_tag(const String &p_id, const String &p_tag) const {
    const ItemInfo* info = get_item_info(p_id);
    if (!info) return false;
    
    TagRegistry *reg = TagRegistry::get_singleton();
    if (!reg) return false;
    
    uint16_t tag_id = reg->get_tag_id(p_tag);
    return TagRegistry::has_tag(tag_id, info->tags);
}

bool ItemDb::has_capability(
    const String& p_id,
    const String& p_capability
) const {
    const ItemInfo* info = get_item_info(p_id);
    const ItemCapability capability =
        capability_from_string(p_capability);
    return info && capability != ItemCapability::NONE &&
        item_has_capability(*info, capability);
}

Array ItemDb::get_capabilities(const String& p_id) const {
    Array result;
    const ItemInfo* info = get_item_info(p_id);
    if (!info) return result;
    if (item_has_capability(*info, ItemCapability::WEAPON)) {
        result.push_back("weapon");
    }
    if (item_has_capability(*info, ItemCapability::CLOTHING)) {
        result.push_back("clothing");
    }
    if (item_has_capability(*info, ItemCapability::LIGHT)) {
        result.push_back("light");
    }
    return result;
}

Dictionary ItemDb::get_clothing_data(const String &p_id) const {
    const ItemInfo* info = get_item_info(p_id);
    if (info && !info->clothing_slots.empty()) return clothing_slot_to_dictionary(info->clothing_slots.front());
    return Dictionary();
}

Array ItemDb::get_clothing_slots(const String &p_id) const {
    Array slots;
    const ItemInfo* info = get_item_info(p_id);
    if (!info) return slots;

    for (const ClothingSlotInfo& slot : info->clothing_slots) {
        slots.push_back(clothing_slot_to_dictionary(slot));
    }
    return slots;
}

const std::vector<ClothingSlotInfo>* ItemDb::get_clothing_slots_info(const String &p_id) const {
    const ItemInfo* info = get_item_info(p_id);
    if (!info || !item_has_capability(*info, ItemCapability::CLOTHING)) {
        return nullptr;
    }
    return &info->clothing_slots;
}

Dictionary ItemDb::get_weapon_data(const String &p_id) const {
    Dictionary result;
    const WeaponItemInfo* weapon = get_weapon_info(p_id);
    if (!weapon) return result;
    result["attack_profile"] = weapon->attack_profile;
    result["damage"] = weapon->damage;
    result["manipulation_load"] = weapon->manipulation_load;
    result["reach"] = weapon->reach;
    return result;
}

const WeaponItemInfo* ItemDb::get_weapon_info(const String& p_id) const {
    const ItemInfo* info = get_item_info(p_id);
    if (!info || !item_has_capability(*info, ItemCapability::WEAPON)) {
        return nullptr;
    }
    return &info->weapon;
}

const LightEmissionInfo* ItemDb::get_light_info(
    const String& p_id
) const {
    const ItemInfo* info = get_item_info(p_id);
    if (!info || !item_has_capability(*info, ItemCapability::LIGHT)) {
        return nullptr;
    }
    return &info->light;
}

const LightEmissionInfo* ItemDb::get_light_info(uint16_t p_id) const {
    const ItemInfo* info = get_item_info(p_id);
    if (!info || !item_has_capability(*info, ItemCapability::LIGHT)) {
        return nullptr;
    }
    return &info->light;
}

String ItemDb::get_item_category(const String &p_id) const {
    const ItemInfo* info = get_item_info(p_id);
    return info ? info->category_id : "";
}

}
