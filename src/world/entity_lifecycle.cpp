#include "entity_lifecycle.h"

#include "entity_archive.h"
#include "world_bubble.h"
#include "turn_scheduler.h"
#include "core/id_registry.h"
#include "core/rng.h"
#include "core/world_coords.h"
#include "data/loot_db.h"
#include "data/race_db.h"
#include "entities/entity.h"
#include "entities/entity_ledger.h"
#include "entities/entity_pool.h"
#include "entities/entity_tracker.h"

#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace {

void drop_entity_contents(uint32_t entity_id, EntityLedger& ledger, WorldBubble& bubble, uint32_t world_seed) {
    Entity* entity = ledger.get_entity_pool().get_entity(entity_id);
    if (!entity) return;

    Vector2i pos(entity->x, entity->y);

    const InventoryData* inventory = ledger.try_get_inventory(entity_id);
    if (inventory) {
        for (const auto& item : inventory->items) {
            if (item.amount > 0) {
                bubble.drop_item(pos, item.id, item.amount);
            }
        }
    }

    const AnatomyData* anatomy = ledger.try_get_anatomy(entity_id);
    if (!anatomy) return;

    RaceDb* race_db = RaceDb::get_singleton();
    if (!race_db) return;

    const RaceInfo* race = race_db->get_race_info(anatomy->race_id);
    if (!race) return;

    if (!race->corpse_item.is_empty()) {
        IdRegistry* reg = IdRegistry::get_singleton();
        if (reg) {
            uint16_t corpse_id = reg->get_id(race->corpse_item);
            if (corpse_id != 0) {
                bubble.drop_item(pos, corpse_id, 1);
            }
        }
    }

    if (race->death_loot_table != 0) {
        LootDb* loot_db = LootDb::get_singleton();
        if (loot_db) {
            Rng::Seeded loot_rng = Rng::at(world_seed, pos, Rng::ENTITY_LOOT, entity_id);
            std::vector<LootStack> stacks;
            loot_db->roll_table(race->death_loot_table, loot_rng, stacks);
            for (const LootStack& stack : stacks) {
                if (stack.item_id != 0 && stack.amount > 0) {
                    bubble.drop_item(pos, stack.item_id, stack.amount);
                }
            }
        }
    }
}

void warn_unschedulable(uint32_t entity_id, const char* action) {
    UtilityFunctions::printerr(
        String("[EntityLifecycle] ")
        + action
        + String(" entity ")
        + String::num_int64(entity_id)
        + String(" has incomplete actor components and will not be scheduled.")
    );
}

}

bool EntityLifecycle::activate_entity(
    uint32_t entity_id,
    const Vector2i& pos,
    float initial_turn_time,
    EntityLedger& ledger,
    EntityTracker& tracker,
    WorldBubble& bubble,
    TurnScheduler& scheduler
) {
    Entity* entity = ledger.get_entity_pool().get_entity(entity_id);
    if (!entity) return false;

    if (!bubble.set_entity(pos.x, pos.y, entity_id)) {
        scheduler.remove(entity_id);
        return false;
    }
    if (!tracker.insert(entity_id, pos)) {
        bubble.remove_entity(pos.x, pos.y);
        scheduler.remove(entity_id);
        return false;
    }

    if (entity->next_turn_time < initial_turn_time) {
        entity->next_turn_time = initial_turn_time;
    }

    if (!ledger.is_schedulable_actor(entity_id)) {
        warn_unschedulable(entity_id, "activate");
        scheduler.remove(entity_id);
        return false;
    }

    scheduler.push(entity_id, entity->next_turn_time);
    return true;
}

bool EntityLifecycle::freeze_entity(
    uint32_t entity_id,
    EntityArchive& archive,
    EntityLedger& ledger,
    EntityTracker& tracker,
    WorldBubble& bubble,
    TurnScheduler& scheduler
) {
    Entity* entity = ledger.get_entity_pool().get_entity(entity_id);
    if (!entity) return false;

    uint64_t packed = WorldCoords::pack_coords(entity->x, entity->y);
    archive.freeze_entity(packed, ledger.serialize_entity(entity_id));
    scheduler.remove(entity_id);
    bubble.remove_entity(entity->x, entity->y);
    tracker.remove(entity_id);
    ledger.destroy_entity(entity_id);
    return true;
}

uint32_t EntityLifecycle::thaw_entity(
    uint64_t packed_pos,
    EntityArchive& archive,
    EntityLedger& ledger,
    EntityTracker& tracker,
    WorldBubble& bubble,
    TurnScheduler& scheduler,
    float minimum_turn_time
) {
    Dictionary data = archive.get_frozen_entity_at(packed_pos);
    if (data.is_empty()) return EntityPool::INVALID_ID;

    uint32_t entity_id = ledger.deserialize_entity(data);
    Entity* entity = ledger.get_entity_pool().get_entity(entity_id);
    if (!entity) return EntityPool::INVALID_ID;

    if (entity->next_turn_time < minimum_turn_time) {
        entity->next_turn_time = minimum_turn_time;
    }

    if (!bubble.set_entity(entity->x, entity->y, entity_id)) {
        ledger.destroy_entity(entity_id);
        return EntityPool::INVALID_ID;
    }
    if (!tracker.insert(entity_id, Vector2i(entity->x, entity->y))) {
        bubble.remove_entity(entity->x, entity->y);
        ledger.destroy_entity(entity_id);
        return EntityPool::INVALID_ID;
    }
    archive.remove_frozen_entity(packed_pos);

    if (ledger.is_schedulable_actor(entity_id)) {
        scheduler.push(entity_id, entity->next_turn_time);
    } else {
        warn_unschedulable(entity_id, "thaw");
        scheduler.remove(entity_id);
    }

    return entity_id;
}

bool EntityLifecycle::despawn_entity(
    uint32_t entity_id,
    EntityLedger& ledger,
    EntityTracker& tracker,
    WorldBubble& bubble,
    TurnScheduler& scheduler,
    uint32_t world_seed,
    bool drop_contents
) {
    if (entity_id == EntityPool::PLAYER_ID) return false;

    Entity* entity = ledger.get_entity_pool().get_entity(entity_id);
    if (entity && drop_contents) {
        drop_entity_contents(entity_id, ledger, bubble, world_seed);
    }

    scheduler.remove(entity_id);

    if (entity) {
        bubble.remove_entity(entity->x, entity->y);
        tracker.remove(entity_id);
    }

    ledger.destroy_entity(entity_id);
    return true;
}
