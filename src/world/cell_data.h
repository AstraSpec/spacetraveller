#ifndef SPACETRAVELLER_CELL_DATA_H
#define SPACETRAVELLER_CELL_DATA_H

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>
#include <cstdint>
#include <vector>
#include <unordered_map>

namespace godot {

struct DroppedItem {
    uint16_t id;
    int amount;
};

class CellData {
    std::unordered_map<uint64_t, std::vector<DroppedItem>> dropped_items;

public:
    CellData() = default;
    ~CellData() = default;

    // Core state
    void add_item(uint64_t key, uint16_t item_id, int amount);
    int remove_item(uint64_t key, uint16_t item_id, int amount); // returns amount actually removed
    int peek_item_amount(uint64_t key, uint16_t item_id) const;  // 0 if not present
    
    const std::vector<DroppedItem>* get_items(uint64_t key) const;
    bool has_items(uint64_t key) const;
    const DroppedItem* get_top_item(uint64_t key) const; // for renderer

    // Persistence
    Dictionary serialize() const;
    void deserialize(const Dictionary& data);
};

}
#endif // SPACETRAVELLER_CELL_DATA_H