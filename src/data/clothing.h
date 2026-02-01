#ifndef SPACETRAVELLER_CLOTHING_H
#define SPACETRAVELLER_CLOTHING_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>
#include <map>
#include <string>

namespace godot {

class Clothing : public Node {
    GDCLASS(Clothing, Node)

private:
    // PartInstance Index -> Layer -> ItemID
    std::map<int, std::map<String, String>> equipped_items;

protected:
    static void _bind_methods();

public:
    Clothing();
    ~Clothing();

    bool equip_item(const String &p_item_id, int p_part_index);
    bool unequip_item(const String &p_item_id);
    bool is_equipped(const String &p_item_id) const;
    
    float get_total_armor() const;
    Array get_equipped_items_list() const;
    Dictionary get_equipped_at(int p_part_index, const String &p_layer) const;
};

}

#endif // ! SPACETRAVELLER_CLOTHING_H
