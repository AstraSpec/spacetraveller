#include "entity_factory.h"
#include "entity_ledger.h"
#include "entity_pool.h"
#include "world/world_bubble.h"
#include "world/turn_scheduler.h"
#include "data/race_db.h"
#include "data/item_db.h"
#include "core/id_registry.h"
#include "components/locomotion.h"
#include "components/perception.h"
#include "components/ai_controller.h"

#include <cstdlib>

using namespace godot;

static void register_and_schedule(uint32_t id, const Vector2i& pos,
                                  EntityLedger& ledger, WorldBubble& bubble, TurnScheduler& scheduler) {
    bubble.set_entity(pos.x, pos.y, id);
    Entity* entity = ledger.get_entity_pool().get_entity(id);
    if (entity) {
        scheduler.push(id, entity->next_turn_time);
    }
}

uint32_t EntityFactory::create_player(const String& race_id, const Vector2i& pos,
                                      EntityLedger& ledger, WorldBubble& bubble, TurnScheduler& scheduler) {
    RaceDb* race_db = RaceDb::get_singleton();
    if (!race_db) return EntityPool::PLAYER_ID;

    Vector2i atlas = race_db->get_atlas_coords(race_id);
    uint32_t id = ledger.spawn_player(pos, atlas.x, atlas.y);

    LocomotionData& loco = ledger.locomotion_data[id];
    Locomotion::init(loco, 1.0f);

    ledger.combat_style[id] = "default";

    register_and_schedule(id, pos, ledger, bubble, scheduler);
    return id;
}

uint32_t EntityFactory::create_npc(const String& race_id, const Vector2i& pos,
                                    EntityLedger& ledger, WorldBubble& bubble, TurnScheduler& scheduler) {
    RaceDb* race_db = RaceDb::get_singleton();
    if (!race_db) return EntityPool::INVALID_ID;

    const RaceInfo* race = race_db->get_race_info(race_id);
    if (!race) return EntityPool::INVALID_ID;

    uint32_t id = ledger.spawn_entity(pos, race->atlas.x, race->atlas.y, race_id);
    if (id == EntityPool::INVALID_ID) return id;

    LocomotionData& loco = ledger.locomotion_data[id];
    Locomotion::init(loco, race->speed);

    ledger.combat_style[id] = race->combat_style;

    AIData& ai = ledger.ai_data[id];
    ai.state = AIState::WANDER;
    ai.wander_center = pos;
    ai.wander_radius = 4.0f;
    ai.stuck_counter = 0;

    if (race->perception_tier == "full_occlusion") {
        ai.perception_tier = PerceptionTier::FULL_OCCLUSION;
    } else {
        ai.perception_tier = PerceptionTier::RAYCAST;
    }

    ledger.perception_memory[id] = PerceptionMemory{};

    ItemDb* item_db = ItemDb::get_singleton();
    if (item_db) {
        Array item_ids = item_db->get_ids();
        if (item_ids.size() > 0) {
            int rand_idx = abs((int)(id * 7 + pos.x * 13 + pos.y * 31)) % item_ids.size();
            String random_item = item_ids[rand_idx];
            ledger.add_inventory_item(id, random_item, 1);
        }
    }

    register_and_schedule(id, pos, ledger, bubble, scheduler);
    return id;
}
