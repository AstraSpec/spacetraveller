#ifndef SPACETRAVELLER_ALLEGIANCE_H
#define SPACETRAVELLER_ALLEGIANCE_H

#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <cstdint>
#include <unordered_map>

namespace godot {

enum class EntityRelation {
    NEUTRAL,
    FRIENDLY,
    HOSTILE
};

struct AllegianceData {
    String faction_id = "unaffiliated";
    std::unordered_map<uint32_t, EntityRelation> personal_relations;
};

namespace Allegiance {
    EntityRelation relation_from_string(const String& value);
    String relation_to_string(EntityRelation value);
    Dictionary serialize(const AllegianceData& data);
    void deserialize(AllegianceData& data, const Dictionary& dict);
}

}

#endif // SPACETRAVELLER_ALLEGIANCE_H
