#include "world_save_serializer.h"

#include "entity_archive.h"
#include "quest_tracker.h"
#include "world_bubble.h"
#include "world_generator.h"
#include "world_spawn_state.h"
#include "entities/entity_ledger.h"

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <unordered_map>

using namespace godot;

Dictionary WorldSaveSerializer::build_save_data(
    int world_seed,
    const WorldGenerator& generator,
    const WorldBubble& bubble,
    const EntityLedger& entity_ledger,
    const EntityArchive& entity_archive,
    const WorldSpawnState& spawn_state,
    const QuestTracker* quest_tracker
) {
    Dictionary data;
    data["seed"] = world_seed;

    Dictionary chunks;
    const auto& region_chunks = generator.get_region_chunks();
    for (auto const& pair : region_chunks) {
        chunks[pair.first] = static_cast<int>(pair.second);
    }
    data["region_chunks"] = chunks;

    data["dropped_items"] = bubble.serialize_ground_items();
    data["tile_id_cache"] = bubble.get_tile_id_cache(WorldBubble::LAYER_TILE);
    data["seen_cells"] = bubble.get_seen_cells();
    data["entity_ledger"] = entity_ledger.serialize();
    data["frozen_entities"] = entity_archive.serialize();
    data["world_spawn_state"] = spawn_state.serialize();

    if (quest_tracker) {
        data["quest_tracker"] = quest_tracker->serialize();
    }

    return data;
}

void WorldSaveSerializer::load_save_data(
    const Dictionary& data,
    int& world_seed,
    WorldGenerator& generator,
    WorldBubble& bubble,
    EntityLedger& entity_ledger,
    EntityArchive& entity_archive,
    WorldSpawnState& spawn_state,
    QuestTracker* quest_tracker
) {
    world_seed = data.get("seed", 0);

    std::unordered_map<uint64_t, uint32_t> region_chunks;
    Dictionary chunks = data.get("region_chunks", Dictionary());
    Array chunk_keys = chunks.keys();
    for (int i = 0; i < chunk_keys.size(); i++) {
        Variant key_var = chunk_keys[i];
        uint64_t key;
        if (key_var.get_type() == Variant::STRING) {
            key = static_cast<uint64_t>(((String)key_var).to_int());
        } else {
            key = static_cast<uint64_t>(static_cast<int64_t>(key_var));
        }
        region_chunks[key] = static_cast<uint32_t>(static_cast<int>(chunks[key_var]));
    }
    generator.set_region_chunks(region_chunks);

    bubble.deserialize_ground_items(data.get("dropped_items", Dictionary()));
    bubble.set_tile_id_cache(data.get("tile_id_cache", Dictionary()), WorldBubble::LAYER_TILE);
    bubble.set_seen_cells(data.get("seen_cells", Array()));
    entity_ledger.deserialize(data.get("entity_ledger", Dictionary()));
    entity_archive.deserialize(data.get("frozen_entities", Dictionary()));

    Dictionary spawn_state_data = data.get("world_spawn_state", Dictionary());
    if (spawn_state_data.is_empty()) {
        spawn_state_data = data.get("entity_spawn_tracker", Dictionary());
    }
    spawn_state.deserialize(spawn_state_data);

    if (quest_tracker) {
        quest_tracker->deserialize(data.get("quest_tracker", Dictionary()));
    }
}
