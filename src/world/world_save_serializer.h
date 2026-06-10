#ifndef SPACETRAVELLER_WORLD_SAVE_SERIALIZER_H
#define SPACETRAVELLER_WORLD_SAVE_SERIALIZER_H

#include <godot_cpp/variant/dictionary.hpp>

namespace godot {

class EntityArchive;
class EntityLedger;
class QuestTracker;
class WorldBubble;
class WorldGenerator;
class WorldSpawnState;

namespace WorldSaveSerializer {
    Dictionary build_save_data(
        int world_seed,
        const WorldGenerator& generator,
        const WorldBubble& bubble,
        const EntityLedger& entity_ledger,
        const EntityArchive& entity_archive,
        const WorldSpawnState& spawn_state,
        const QuestTracker* quest_tracker
    );

    void load_save_data(
        const Dictionary& data,
        int& world_seed,
        WorldGenerator& generator,
        WorldBubble& bubble,
        EntityLedger& entity_ledger,
        EntityArchive& entity_archive,
        WorldSpawnState& spawn_state,
        QuestTracker* quest_tracker
    );
}

}

#endif // SPACETRAVELLER_WORLD_SAVE_SERIALIZER_H
