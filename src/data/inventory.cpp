#include "inventory.h"
#include "item_db.h"
#include "id_registry.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

void Inventory::_bind_methods() {
    ClassDB::bind_method(D_METHOD("add_item", "item_id", "amount"), &Inventory::add_item);
    ClassDB::bind_method(D_METHOD("remove_item", "item_id", "amount"), &Inventory::remove_item);
    ClassDB::bind_method(D_METHOD("has_item", "item_id", "amount"), &Inventory::has_item);
    
    ClassDB::bind_method(D_METHOD("get_total_weight"), &Inventory::get_total_weight);
    ClassDB::bind_method(D_METHOD("get_total_volume"), &Inventory::get_total_volume);
    
    ClassDB::bind_method(D_METHOD("get_max_weight"), &Inventory::get_max_weight);
    ClassDB::bind_method(D_METHOD("set_max_weight", "weight"), &Inventory::set_max_weight);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_weight"), "set_max_weight", "get_max_weight");
    
    ClassDB::bind_method(D_METHOD("get_max_volume"), &Inventory::get_max_volume);
    ClassDB::bind_method(D_METHOD("set_max_volume", "volume"), &Inventory::set_max_volume);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_volume"), "set_max_volume", "get_max_volume");

    ClassDB::bind_method(D_METHOD("get_items_list"), &Inventory::get_items_list);

    ADD_SIGNAL(MethodInfo("item_added", PropertyInfo(Variant::STRING, "item_id"), PropertyInfo(Variant::INT, "amount")));
    ADD_SIGNAL(MethodInfo("item_removed", PropertyInfo(Variant::STRING, "item_id"), PropertyInfo(Variant::INT, "amount")));
    ADD_SIGNAL(MethodInfo("inventory_changed"));
}

Inventory::Inventory() {}
Inventory::~Inventory() {}

void Inventory::update_totals() {
    current_weight = 0.0f;
    current_volume = 0.0f;
    ItemDb* db = ItemDb::get_singleton();
    if (!db) return;

    for (const auto& item : items) {
        const ItemInfo* info = db->get_item_info(item.id);
        if (info) {
            current_weight += info->weight * item.amount;
            current_volume += info->volume * item.amount;
        }
    }
}

bool Inventory::add_item(const String &p_item_id, int p_amount) {
    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (!id_reg) return false;
    return add_item_numeric(id_reg->get_id(p_item_id), p_amount);
}

bool Inventory::add_item_numeric(uint16_t p_id, int p_amount) {
    ItemDb* db = ItemDb::get_singleton();
    if (!db) return false;

    const ItemInfo* info = db->get_item_info(p_id);
    if (!info) return false;

    // CDDA Capacity Check
    float added_weight = info->weight * p_amount;
    float added_volume = info->volume * p_amount;

    if (current_weight + added_weight > max_weight || current_volume + added_volume > max_volume) {
        return false;
    }

    // Stack check
    for (auto& item : items) {
        if (item.id == p_id) {
            item.amount += p_amount;
            update_totals();
            emit_signal("item_added", IdRegistry::get_singleton()->get_string(p_id), p_amount);
            emit_signal("inventory_changed");
            return true;
        }
    }

    items.push_back({p_id, p_amount});
    update_totals();
    emit_signal("item_added", IdRegistry::get_singleton()->get_string(p_id), p_amount);
    emit_signal("inventory_changed");
    return true;
}

bool Inventory::remove_item(const String &p_item_id, int p_amount) {
    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (!id_reg) return false;
    uint16_t id = id_reg->get_id(p_item_id);

    for (auto it = items.begin(); it != items.end(); ++it) {
        if (it->id == id) {
            if (it->amount >= p_amount) {
                it->amount -= p_amount;
                if (it->amount == 0) {
                    items.erase(it);
                }
                update_totals();
                emit_signal("item_removed", p_item_id, p_amount);
                emit_signal("inventory_changed");
                return true;
            }
            return false;
        }
    }
    return false;
}

bool Inventory::has_item(const String &p_item_id, int p_amount) const {
    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (!id_reg) return false;
    uint16_t id = id_reg->get_id(p_item_id);

    for (const auto& item : items) {
        if (item.id == id) {
            return item.amount >= p_amount;
        }
    }
    return false;
}

Array Inventory::get_items_list() const {
    Array list;
    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (!id_reg) return list;

    for (const auto& item : items) {
        Dictionary d;
        d["id"] = id_reg->get_string(item.id);
        d["amount"] = item.amount;
        list.push_back(d);
    }
    return list;
}

}
