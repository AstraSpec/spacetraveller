#ifndef SPACETRAVELLER_ENTITY_ARCHIVE_H
#define SPACETRAVELLER_ENTITY_ARCHIVE_H

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace godot {

class EntityArchive {
private:
    std::unordered_map<uint64_t, Dictionary> frozen_entities;

public:
    void freeze_entity(uint64_t packed_pos, const Dictionary& entity_data);
    Dictionary get_frozen_entity_at(uint64_t packed_pos) const;
    bool has_frozen_entity(uint64_t packed_pos) const;
    void remove_frozen_entity(uint64_t packed_pos);
    std::vector<uint64_t> get_frozen_keys_in_range(const Vector2i& center, int radius) const;
    Dictionary serialize() const;
    void deserialize(const Dictionary& data);
    void clear();
};

}

#endif // SPACETRAVELLER_ENTITY_ARCHIVE_H
