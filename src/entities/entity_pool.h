#ifndef SPACETRAVELLER_ENTITY_POOL_H
#define SPACETRAVELLER_ENTITY_POOL_H

#include "entity.h"
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>
#include <cstdint>
#include <vector>
#include <unordered_map>

namespace godot {

class EntityPool {
public:
    static constexpr uint32_t INVALID_ID = 0;

    EntityPool() = default;

    uint32_t create_entity(int x, int y, uint16_t atlas_x, uint16_t atlas_y);
    void destroy_entity(uint32_t id);

    Entity* get_entity(uint32_t id);
    const Entity* get_entity(uint32_t id) const;

    const std::vector<Entity>& get_all() const { return entities; }

    size_t living_count() const;

    Dictionary serialize() const;
    void deserialize(const Dictionary& data);

private:
    std::vector<Entity> entities;
    std::vector<uint32_t> free_slots;
    std::unordered_map<uint32_t, uint32_t> id_to_slot;
    uint32_t next_id = 1;
};

}

#endif // SPACETRAVELLER_ENTITY_POOL_H