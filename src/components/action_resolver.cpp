#include "action_resolver.h"
#include "entities/entity.h"
#include "world/world_bubble.h"
#include "locomotion.h"
#include "data/tile_db.h"
#include <cmath>

using namespace godot;

float ActionResolver::resolve_move(const Intent& intent, Entity& entity,
                                    WorldBubble& bubble, LocomotionData& loco) {
    int dx = abs(intent.target.x - entity.x);
    int dy = abs(intent.target.y - entity.y);

    if (dx > 1 || dy > 1 || (dx == 0 && dy == 0)) return 0.0f;

    TileDb* tile_db = TileDb::get_singleton();
    if (tile_db) {
        uint16_t tile_id = bubble.query_tile_id(intent.target.x, intent.target.y);
        if (tile_id != 0) {
            const TileInfo* info = tile_db->get_tile_info(tile_id);
            if (info && info->solid) return 0.0f;
        }
    }

    const WorldBubble::CellEntity* occupant = bubble.get_entity_at(intent.target.x, intent.target.y);
    if (occupant) return 0.0f;

    int old_x = entity.x, old_y = entity.y;
    bubble.update_entity_position(old_x, old_y, intent.target.x, intent.target.y, entity.id);
    entity.x = intent.target.x;
    entity.y = intent.target.y;

    Locomotion::advance_step(loco);

    return Locomotion::get_step_cost(old_x, old_y, intent.target.x, intent.target.y);
}
