#ifndef SPACETRAVELLER_ANATOMY_H
#define SPACETRAVELLER_ANATOMY_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/array.hpp>
#include <vector>

namespace godot {

struct PartInstance {
    String type_id;
    int parent_index = -1;
    float integrity = 1.0f;
    int local_index = 0; // index among siblings of same type
};

class Anatomy : public Node {
    GDCLASS(Anatomy, Node)

private:
    std::vector<PartInstance> instances;
    String race_id;

protected:
    static void _bind_methods();

public:
    Anatomy();
    ~Anatomy();

    void initialize_from_race(const String &p_race_id);
    
    int find_part_of_type(const String &p_type_id, int p_skip_count = 0) const;
    bool is_part_functional(int p_index) const;
    int get_part_count() const { return static_cast<int>(instances.size()); }
    
    String get_part_type_id(int p_index) const;
    String get_part_name(int p_index) const;
    int get_part_parent(int p_index) const;
    float get_part_integrity(int p_index) const;
    void set_part_integrity(int p_index, float p_integrity);

    Array get_functional_parts_list() const;
};

}

#endif // ! SPACETRAVELLER_ANATOMY_H
