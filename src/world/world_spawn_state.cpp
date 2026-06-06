#include "world_spawn_state.h"
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace godot {

bool WorldSpawnState::has_attempted(uint64_t p_packed_pos) const {
    return attempted_cells.find(p_packed_pos) != attempted_cells.end();
}

void WorldSpawnState::mark_attempted(uint64_t p_packed_pos) {
    attempted_cells.insert(p_packed_pos);
}

void WorldSpawnState::clear() {
    attempted_cells.clear();
}

Dictionary WorldSpawnState::serialize() const {
    Dictionary data;
    Array cells;
    for (uint64_t key : attempted_cells) {
        cells.push_back(Variant(static_cast<int64_t>(key)));
    }
    data[String("attempted_cells")] = cells;
    return data;
}

void WorldSpawnState::deserialize(const Dictionary &p_data) {
    attempted_cells.clear();
    Array cells = p_data.get(String("attempted_cells"), Array());
    for (int i = 0; i < cells.size(); i++) {
        Variant key_var = cells[i];
        uint64_t key = 0;
        if (key_var.get_type() == Variant::STRING) {
            key = ((String)key_var).to_int();
        } else {
            key = static_cast<uint64_t>(static_cast<int64_t>(key_var));
        }
        attempted_cells.insert(key);
    }
}

}
