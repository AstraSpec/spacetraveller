#include "cell_data.h"
#include "core/id_registry.h"
#include "data/item_db.h"
#include <algorithm>

using namespace godot;

void CellData::add_item(uint64_t key, uint16_t item_id, int amount) {
    auto& stack = dropped_items[key];
    for (auto& item : stack) {
        if (item.id == item_id) {
            item.amount += amount;
            return;
        }
    }

    // Items tagged ALWAYS_TOP are inserted at the front so get_top_item returns them
    bool insert_front = false;
    ItemDb* db = ItemDb::get_singleton();
    if (db) {
        const ItemInfo* info = db->get_item_info(item_id);
        if (info) {
            TagRegistry* reg = TagRegistry::get_singleton();
            if (reg) {
                uint16_t tag_id = reg->get_tag_id("ALWAYS_TOP");
                insert_front = TagRegistry::has_tag(tag_id, info->tags);
            }
        }
    }

    if (insert_front) {
        stack.insert(stack.begin(), {item_id, amount});
    } else {
        stack.push_back({item_id, amount});
    }
}

int CellData::remove_item(uint64_t key, uint16_t item_id, int amount) {
    auto it = dropped_items.find(key);
    if (it == dropped_items.end()) return 0;

    for (auto item_it = it->second.begin(); item_it != it->second.end(); ++item_it) {
        if (item_it->id == item_id) {
            int to_remove = std::min(amount, item_it->amount);
            item_it->amount -= to_remove;
            if (item_it->amount <= 0) {
                it->second.erase(item_it);
            }
            if (it->second.empty()) {
                dropped_items.erase(it);
            }
            return to_remove;
        }
    }
    return 0;
}

int CellData::peek_item_amount(uint64_t key, uint16_t item_id) const {
    auto it = dropped_items.find(key);
    if (it == dropped_items.end()) return 0;
    for (const auto& item : it->second) {
        if (item.id == item_id) return item.amount;
    }
    return 0;
}

const std::vector<DroppedItem>* CellData::get_items(uint64_t key) const {
    auto it = dropped_items.find(key);
    return (it != dropped_items.end()) ? &it->second : nullptr;
}

bool CellData::has_items(uint64_t key) const {
    auto it = dropped_items.find(key);
    return it != dropped_items.end() && !it->second.empty();
}

const DroppedItem* CellData::get_top_item(uint64_t key) const {
    auto it = dropped_items.find(key);
    if (it != dropped_items.end() && !it->second.empty()) return &it->second[0];
    return nullptr;
}

Dictionary CellData::serialize() const {
    Dictionary data;
    IdRegistry* id_reg = IdRegistry::get_singleton();
    for (const auto& [key, list] : dropped_items) {
        Array a;
        for (const auto& item : list) {
            Dictionary d;
            d["id"] = id_reg ? id_reg->get_string(item.id) : String::num_int64(item.id);
            d["amount"] = item.amount;
            a.push_back(d);
        }
        data[key] = a;
    }
    return data;
}

void CellData::deserialize(const Dictionary& data) {
    dropped_items.clear();
    Array keys = data.keys();
    IdRegistry* id_reg = IdRegistry::get_singleton();
    for (int i = 0; i < keys.size(); i++) {
        Variant key_var = keys[i];
        uint64_t key = (key_var.get_type() == Variant::STRING) ? ((String)key_var).to_int() : (uint64_t)key_var;
        Array a = data[key_var];
        std::vector<DroppedItem> list;
        for (int j = 0; j < a.size(); j++) {
            Dictionary d = a[j];
            uint16_t id = id_reg ? id_reg->get_id(d.get("id", "")) : (uint16_t)d.get("id", 0);
            list.push_back({id, (int)d.get("amount", 0)});
        }
        dropped_items[key] = std::move(list);
    }
}