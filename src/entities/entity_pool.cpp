#include "entity_pool.h"
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/string.hpp>

using namespace godot;

uint32_t EntityPool::create_entity(int x, int y, uint16_t atlas_x, uint16_t atlas_y) {
    uint32_t id = next_id++;
    uint32_t slot;

    if (!free_slots.empty()) {
        slot = free_slots.back();
        free_slots.pop_back();
        entities[slot] = {id, x, y, 0, 0.0f, atlas_x, atlas_y};
    } else {
        slot = static_cast<uint32_t>(entities.size());
        entities.push_back({id, x, y, 0, 0.0f, atlas_x, atlas_y});
    }

    id_to_slot[id] = slot;
    return id;
}

uint32_t EntityPool::create_player_entity(int x, int y, uint16_t atlas_x, uint16_t atlas_y) {
    uint32_t slot;

    auto it = id_to_slot.find(PLAYER_ID);
    if (it != id_to_slot.end()) {
        slot = it->second;
        entities[slot] = {PLAYER_ID, x, y, 0, 0.0f, atlas_x, atlas_y};
    } else {
        slot = static_cast<uint32_t>(entities.size());
        entities.push_back({PLAYER_ID, x, y, 0, 0.0f, atlas_x, atlas_y});
        id_to_slot[PLAYER_ID] = slot;
    }
    return PLAYER_ID;
}

void EntityPool::destroy_entity(uint32_t id) {
    if (id == PLAYER_ID) return;
    auto it = id_to_slot.find(id);
    if (it != id_to_slot.end()) {
        free_slots.push_back(it->second);
        id_to_slot.erase(it);
    }
}

Entity* EntityPool::get_entity(uint32_t id) {
    auto it = id_to_slot.find(id);
    return (it != id_to_slot.end()) ? &entities[it->second] : nullptr;
}

const Entity* EntityPool::get_entity(uint32_t id) const {
    auto it = id_to_slot.find(id);
    return (it != id_to_slot.end()) ? &entities[it->second] : nullptr;
}

size_t EntityPool::living_count() const {
    return id_to_slot.size();
}

Dictionary EntityPool::serialize() const {
    Dictionary data;
    Array arr;
    for (const auto& pair : id_to_slot) {
        const Entity& e = entities[pair.second];
        Dictionary d;
        d[String("id")] = static_cast<int64_t>(e.id);
        d[String("x")] = e.x;
        d[String("y")] = e.y;
        d[String("component_mask")] = static_cast<int64_t>(e.component_mask);
        d[String("next_turn_time")] = e.next_turn_time;
        d[String("atlas_x")] = static_cast<int>(e.atlas_x);
        d[String("atlas_y")] = static_cast<int>(e.atlas_y);
        arr.push_back(d);
    }
    data[String("entities")] = arr;
    data[String("next_id")] = static_cast<int64_t>(next_id);
    return data;
}

void EntityPool::deserialize(const Dictionary& data) {
    entities.clear();
    free_slots.clear();
    id_to_slot.clear();
    next_id = static_cast<uint32_t>(static_cast<int64_t>(data.get(String("next_id"), static_cast<int64_t>(1))));

    Array arr = data.get(String("entities"), Array());
    for (int i = 0; i < arr.size(); i++) {
        Dictionary d = arr[i];
        Entity e;
        e.id = static_cast<uint32_t>(static_cast<int64_t>(d.get(String("id"), static_cast<int64_t>(0))));
        e.x = static_cast<int>(d.get(String("x"), 0));
        e.y = static_cast<int>(d.get(String("y"), 0));
        e.component_mask = static_cast<uint32_t>(static_cast<int64_t>(d.get(String("component_mask"), static_cast<int64_t>(0))));
        e.next_turn_time = static_cast<float>(static_cast<double>(d.get(String("next_turn_time"), 0.0)));
        e.atlas_x = static_cast<uint16_t>(static_cast<int>(d.get(String("atlas_x"), 0)));
        e.atlas_y = static_cast<uint16_t>(static_cast<int>(d.get(String("atlas_y"), 0)));
        uint32_t slot = static_cast<uint32_t>(entities.size());
        entities.push_back(e);
        id_to_slot[e.id] = slot;
    }
}
