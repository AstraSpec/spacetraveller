#include "components/action_planner.h"

using namespace godot;

ActionPlan ActionPlanner::plan_player_intent(
    const Intent& raw_intent,
    WorldBubble& bubble,
    uint32_t player_id,
    const String& player_faction,
    const String& target_faction
) {
    ActionPlan plan;
    plan.intent = raw_intent;

    if (raw_intent.type != IntentType::MOVE) {
        return plan;
    }

    const WorldBubble::CellEntity* occupant = bubble.get_entity_at(raw_intent.target.x, raw_intent.target.y);
    if (occupant && occupant->entity_id != player_id) {
        if (Faction::are_hostile(player_faction, target_faction)) {
            plan.intent.type = IntentType::ATTACK;
        } else {
            plan.should_interact = true;
            plan.interact_target = occupant->entity_id;
        }
        return plan;
    }

    if (!occupant) {
        TileDb* tile_db = TileDb::get_singleton();
        TagRegistry* tag_reg = TagRegistry::get_singleton();
        uint16_t can_open = tag_reg ? tag_reg->get_tag_id("CAN_OPEN") : 0;
        uint16_t tile_id = bubble.query_tile_id(raw_intent.target.x, raw_intent.target.y);
        const TileInfo* info = tile_db ? tile_db->get_tile_info(tile_id) : nullptr;
        if (info && can_open != 0 && info->opens_to != 0 && tile_db->has_tag(tile_id, can_open)) {
            plan.intent.type = IntentType::OPEN;
        }
    }

    return plan;
}
