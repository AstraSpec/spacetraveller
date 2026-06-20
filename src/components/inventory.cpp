#include "inventory.h"
#include "data/item_db.h"
#include "core/id_registry.h"

namespace godot {

void Inventory::init(InventoryData& data, float max_weight, float max_volume) {
    data.items.clear();
    data.max_weight = max_weight;
    data.max_volume = max_volume;
    data.current_weight = 0.0f;
    data.current_volume = 0.0f;
}

void Inventory::update_totals(InventoryData& data) {
    data.current_weight = 0.0f;
    data.current_volume = 0.0f;
    ItemDb* db = ItemDb::get_singleton();
    if (!db) return;

    for (const auto& item : data.items) {
        const ItemInfo* info = db->get_item_info(item.id);
        if (info) {
            data.current_weight += info->weight * item.amount;
            data.current_volume += info->volume * item.amount;
        }
    }
}

bool Inventory::add_item(InventoryData& data, uint16_t item_id, int amount) {
    ItemDb* db = ItemDb::get_singleton();
    if (!db) return false;

    const ItemInfo* info = db->get_item_info(item_id);
    if (!info) return false;

    float added_weight = info->weight * amount;
    float added_volume = info->volume * amount;

    if (data.current_weight + added_weight > data.max_weight ||
        data.current_volume + added_volume > data.max_volume) {
        return false;
    }

    for (auto& item : data.items) {
        if (item.id == item_id) {
            item.amount += amount;
            update_totals(data);
            return true;
        }
    }

    data.items.push_back({item_id, amount});
    update_totals(data);
    return true;
}

bool Inventory::add_item_by_string(InventoryData& data, const String& item_id, int amount) {
    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (!id_reg) return false;
    return add_item(data, id_reg->get_id(item_id), amount);
}

bool Inventory::remove_item(InventoryData& data, uint16_t item_id, int amount) {
    for (auto it = data.items.begin(); it != data.items.end(); ++it) {
        if (it->id == item_id) {
            if (it->amount >= amount) {
                it->amount -= amount;
                if (it->amount == 0) {
                    data.items.erase(it);
                }
                update_totals(data);
                return true;
            }
            return false;
        }
    }
    return false;
}

bool Inventory::remove_item_by_string(InventoryData& data, const String& item_id, int amount) {
    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (!id_reg) return false;
    return remove_item(data, id_reg->get_id(item_id), amount);
}

bool Inventory::has_item(const InventoryData& data, uint16_t item_id, int amount) {
    for (const auto& item : data.items) {
        if (item.id == item_id) {
            return item.amount >= amount;
        }
    }
    return false;
}

bool Inventory::has_item_by_string(const InventoryData& data, const String& item_id, int amount) {
    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (!id_reg) return false;
    return has_item(data, id_reg->get_id(item_id), amount);
}

int Inventory::get_item_amount(const InventoryData& data, uint16_t item_id) {
    for (const auto& item : data.items) {
        if (item.id == item_id) return item.amount;
    }
    return 0;
}

Array Inventory::get_items_list(const InventoryData& data) {
    Array list;
    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (!id_reg) return list;

    for (const auto& item : data.items) {
        Dictionary d;
        d["id"] = id_reg->get_string(item.id);
        d["amount"] = item.amount;
        list.push_back(d);
    }
    return list;
}

Dictionary Inventory::serialize(const InventoryData& data) {
    Dictionary d;
    d["max_weight"] = data.max_weight;
    d["max_volume"] = data.max_volume;
    
    Dictionary items;
    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (id_reg) {
        for (const auto& item : data.items) {
            items[id_reg->get_string(item.id)] = item.amount;
        }
    }
    d["items"] = items;
    return d;
}

void Inventory::deserialize(InventoryData& data, const Dictionary& dict) {
    data.items.clear();
    data.max_weight = dict.get("max_weight", 50.0f);
    data.max_volume = dict.get("max_volume", 20.0f);

    Dictionary items = dict.get("items", Dictionary());
    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (!id_reg) return;

    Array keys = items.keys();
    for (int i = 0; i < keys.size(); i++) {
        String id_str = String(keys[i]);
        int amount = int(items[keys[i]]);
        uint16_t id = id_reg->get_id(id_str);
        if (id != 0 && amount > 0) {
            data.items.push_back({id, amount});
        }
    }
    update_totals(data);
}

}
