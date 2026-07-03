#include "item_db.h"
#include "core/id_registry.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

template<> ItemDb* DataBase<ItemInfo, ItemDb>::singleton = nullptr;

namespace {

Dictionary clothing_slot_to_dictionary(const ClothingSlotInfo& slot) {
    Dictionary d;
    d["part"] = slot.part;
    d["layer"] = slot.layer;
    d["armor"] = slot.armor;
    return d;
}

bool parse_clothing_slot(const Dictionary& p_data, ClothingSlotInfo& r_slot) {
    String part = String(p_data.get("part", "")).strip_edges();
    if (part.is_empty()) return false;

    r_slot.part = part;
    r_slot.layer = String(p_data.get("layer", "middle")).strip_edges();
    if (r_slot.layer.is_empty()) r_slot.layer = "middle";
    r_slot.armor = static_cast<float>(static_cast<double>(p_data.get("armor", 0.0)));
    return true;
}

std::vector<ClothingSlotInfo> parse_clothing_slots(const Variant& p_clothing) {
    std::vector<ClothingSlotInfo> slots;
    if (p_clothing.get_type() == Variant::DICTIONARY) {
        ClothingSlotInfo slot;
        if (parse_clothing_slot(p_clothing, slot)) {
            slots.push_back(slot);
        }
    } else if (p_clothing.get_type() == Variant::ARRAY) {
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
    ClassDB::bind_method(D_METHOD("get_clothing_data", "id"), &ItemDb::get_clothing_data);
    ClassDB::bind_method(D_METHOD("get_clothing_slots", "id"), &ItemDb::get_clothing_slots);
    ClassDB::bind_method(D_METHOD("get_weapon_data", "id"), &ItemDb::get_weapon_data);
    ClassDB::bind_method(D_METHOD("get_item_type", "id"), &ItemDb::get_item_type);
    ClassDB::bind_method(D_METHOD("get_ids"), &ItemDb::get_ids);
}

ItemDb::ItemDb() {
}

ItemDb::~ItemDb() {
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
    info.clothing_slots = parse_clothing_slots(p_data.get("clothing", Variant()));
    info.weapon_data = p_data.get("weapon", Dictionary());
    info.type = p_data.get("type", "misc");
    info.light = parse_light_emission(p_data.get("light", Variant()));
    
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
    if (!info) return nullptr;
    return &info->clothing_slots;
}

Dictionary ItemDb::get_weapon_data(const String &p_id) const {
    const ItemInfo* info = get_item_info(p_id);
    if (info) return info->weapon_data;
    return Dictionary();
}

String ItemDb::get_item_type(const String &p_id) const {
    const ItemInfo* info = get_item_info(p_id);
    if (info && !info->type.is_empty()) return info->type;
    return "misc";
}

}
