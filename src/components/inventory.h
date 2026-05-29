#ifndef SPACETRAVELLER_INVENTORY_H
#define SPACETRAVELLER_INVENTORY_H

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>
#include <vector>
#include <cstdint>

namespace godot {

struct InventoryItem {
    uint16_t id;
    int amount;
};

struct InventoryData {
    std::vector<InventoryItem> items;
    float max_weight = 50.0f;
    float max_volume = 20.0f;

    float current_weight = 0.0f;
    float current_volume = 0.0f;
};

namespace Inventory {
    void init(InventoryData& data, float max_weight = 50.0f, float max_volume = 20.0f);
    
    void update_totals(InventoryData& data);
    bool add_item(InventoryData& data, uint16_t item_id, int amount);
    bool add_item_by_string(InventoryData& data, const String& item_id, int amount);
    bool remove_item(InventoryData& data, uint16_t item_id, int amount);
    bool remove_item_by_string(InventoryData& data, const String& item_id, int amount);
    
    bool has_item(const InventoryData& data, uint16_t item_id, int amount = 1);
    bool has_item_by_string(const InventoryData& data, const String& item_id, int amount = 1);
    int get_item_amount(const InventoryData& data, uint16_t item_id);
    
    inline float get_total_weight(const InventoryData& data) { return data.current_weight; }
    inline float get_total_volume(const InventoryData& data) { return data.current_volume; }
    inline float get_max_weight(const InventoryData& data) { return data.max_weight; }
    inline float get_max_volume(const InventoryData& data) { return data.max_volume; }
    
    inline void set_max_weight(InventoryData& data, float weight) { data.max_weight = weight; }
    inline void set_max_volume(InventoryData& data, float volume) { data.max_volume = volume; }
    
    Array get_items_list(const InventoryData& data); // For UI
    
    Dictionary serialize(const InventoryData& data);
    void deserialize(InventoryData& data, const Dictionary& dict);
}

}

#endif // ! SPACETRAVELLER_INVENTORY_H