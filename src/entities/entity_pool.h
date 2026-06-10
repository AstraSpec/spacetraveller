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
    static constexpr uint32_t INVALID_ID = INVALID_ENTITY_ID;
    static constexpr uint32_t PLAYER_ID = PLAYER_ENTITY_ID;

    EntityPool() = default;

    uint32_t create_entity(int x, int y, uint16_t atlas_x, uint16_t atlas_y);
    uint32_t create_entity_with_id(uint32_t id, int x, int y, uint16_t atlas_x, uint16_t atlas_y);
    uint32_t create_player_entity(int x, int y, uint16_t atlas_x, uint16_t atlas_y);
    void destroy_entity(uint32_t id);

    Entity* get_entity(uint32_t id);
    const Entity* get_entity(uint32_t id) const;
    bool contains(uint32_t id) const;

    // Low-level storage view. This includes stale slots from destroyed entities;
    // gameplay code should prefer get_live_ids() or collect_live_ids().
    const std::vector<Entity>& get_all() const { return entities; }
    std::vector<uint32_t> get_live_ids() const;
    void collect_live_ids(std::vector<uint32_t>& out) const;

    size_t living_count() const;

    Dictionary serialize() const;
    void deserialize(const Dictionary& data);
    void clear() { entities.clear(); free_slots.clear(); id_to_slot.clear(); next_id = 1; }

private:
    std::vector<Entity> entities;
    std::vector<uint32_t> free_slots;
    std::unordered_map<uint32_t, uint32_t> id_to_slot;
    uint32_t next_id = 1;
};

}

#endif // SPACETRAVELLER_ENTITY_POOL_H
