#ifndef SPACETRAVELLER_WORLD_SPAWN_STATE_H
#define SPACETRAVELLER_WORLD_SPAWN_STATE_H

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <cstdint>
#include <unordered_set>

namespace godot {

class WorldSpawnState {
    std::unordered_set<uint64_t> attempted_cells;

public:
    bool has_attempted(uint64_t p_packed_pos) const;
    void mark_attempted(uint64_t p_packed_pos);
    void clear();

    Dictionary serialize() const;
    void deserialize(const Dictionary &p_data);
};

}

#endif // SPACETRAVELLER_WORLD_SPAWN_STATE_H
