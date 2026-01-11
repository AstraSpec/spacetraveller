#ifndef SPACETRAVELLER_INVENTORY_H
#define SPACETRAVELLER_INVENTORY_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/array.hpp>
#include <vector>
#include <cstdint>

namespace godot {

struct InventoryItem {
    uint16_t id;
    int amount;
};

class Inventory : public Node {
    GDCLASS(Inventory, Node)

private:
    std::vector<InventoryItem> items;
    float max_weight = 50.0f;
    float max_volume = 20.0f;

    float current_weight = 0.0f;
    float current_volume = 0.0f;

    void update_totals();

protected:
    static void _bind_methods();

public:
    Inventory();
    ~Inventory();

    bool add_item(const String &p_item_id, int p_amount);
    bool add_item_numeric(uint16_t p_id, int p_amount);
    bool remove_item(const String &p_item_id, int p_amount);
    
    bool has_item(const String &p_item_id, int p_amount) const;
    
    float get_total_weight() const { return current_weight; }
    float get_total_volume() const { return current_volume; }
    
    float get_max_weight() const { return max_weight; }
    void set_max_weight(float p_weight) { max_weight = p_weight; }
    
    float get_max_volume() const { return max_volume; }
    void set_max_volume(float p_volume) { max_volume = p_volume; }

    Array get_items_list() const; // For UI
};

}

#endif // ! SPACETRAVELLER_INVENTORY_H
