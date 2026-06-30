#include "entity_archive.h"
#include "cell_area.h"
#include "core/world_coords.h"

#include <cstdlib>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>

using namespace godot;

void EntityArchive::freeze_entity(uint64_t packed_pos, const Dictionary& entity_data) {
    frozen_entities[packed_pos] = entity_data;
}

Dictionary EntityArchive::get_frozen_entity_at(uint64_t packed_pos) const {
    auto it = frozen_entities.find(packed_pos);
    if (it != frozen_entities.end()) return it->second;
    return Dictionary();
}

bool EntityArchive::has_frozen_entity(uint64_t packed_pos) const {
    return frozen_entities.find(packed_pos) != frozen_entities.end();
}

void EntityArchive::remove_frozen_entity(uint64_t packed_pos) {
    frozen_entities.erase(packed_pos);
}

std::vector<uint64_t> EntityArchive::get_frozen_keys_in_range(const Vector2i& center, int radius, int z) const {
    std::vector<uint64_t> result;
    for (const auto& [key, data] : frozen_entities) {
        (void)data;
        Vector3i pos = WorldCoords::unpack_coords_3d(key);
        if (pos.z == z && std::abs(pos.x - center.x) <= radius && std::abs(pos.y - center.y) <= radius) {
            result.push_back(key);
        }
    }
    return result;
}

std::vector<uint64_t> EntityArchive::get_frozen_keys_in_area(const CellArea& area) const {
    std::vector<uint64_t> result;
    for (const auto& [key, data] : frozen_entities) {
        (void)data;
        Vector3i pos = WorldCoords::unpack_coords_3d(key);
        if (area.contains_world(pos.x, pos.y, pos.z)) {
            result.push_back(key);
        }
    }
    return result;
}

Dictionary EntityArchive::serialize() const {
    Dictionary data;
    for (const auto& [key, entity_data] : frozen_entities) {
        data[static_cast<int64_t>(key)] = entity_data;
    }
    return data;
}

void EntityArchive::deserialize(const Dictionary& data) {
    frozen_entities.clear();
    Array keys = data.keys();
    for (int i = 0; i < keys.size(); i++) {
        Variant key_var = keys[i];
        uint64_t key;
        if (key_var.get_type() == Variant::STRING) {
            key = static_cast<uint64_t>(((String)key_var).to_int());
        } else {
            key = static_cast<uint64_t>(static_cast<int64_t>(key_var));
        }
        frozen_entities[key] = data[key_var];
    }
}

void EntityArchive::clear() {
    frozen_entities.clear();
}
