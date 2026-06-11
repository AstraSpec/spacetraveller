#include "traversal_rules.h"

#include "components/anatomy.h"
#include "core/tag_registry.h"
#include "data/body_part_db.h"
#include "data/race_db.h"
#include "data/tile_db.h"
#include "data/traversal_profile_db.h"
#include "entities/entity_ledger.h"

using namespace godot;

namespace {

static bool has_functional_body_tag(const AnatomyData& anatomy, const String& tag) {
    return Anatomy::count_functional_parts_with_tag(anatomy, tag) > 0;
}

static bool tile_has_any_tag(const TileInfo& tile, const std::vector<uint16_t>& tags) {
    return TagRegistry::has_tag_any(tile.tags, tags);
}

static bool can_profile_enter_internal(
    const TraversalProfileInfo& profile,
    const TileInfo* tile,
    const AnatomyData* anatomy,
    bool enforce_body_requirement,
    bool ignore_solid = false
) {
    if (!profile.can_move) return false;

    if (enforce_body_requirement && !profile.requires_body_tag.is_empty()) {
        if (!anatomy || !has_functional_body_tag(*anatomy, profile.requires_body_tag)) {
            return false;
        }
    }

    if (!tile) return true;

    if (!profile.blocked_tile_tags.empty() && tile_has_any_tag(*tile, profile.blocked_tile_tags)) {
        return false;
    }

    if (!ignore_solid && tile->solid && !profile.can_enter_solid) {
        return false;
    }

    if (!profile.allowed_tile_tags.empty() && !tile_has_any_tag(*tile, profile.allowed_tile_tags)) {
        return false;
    }

    return true;
}

static const TraversalProfileInfo* get_profile(const String& profile_id) {
    TraversalProfileDb* profile_db = TraversalProfileDb::get_singleton();
    return profile_db ? profile_db->get_profile_info(profile_id.is_empty() ? String("animal_walker") : profile_id) : nullptr;
}

static const TileInfo* get_tile(uint16_t tile_id) {
    if (tile_id == 0) return nullptr;
    TileDb* tile_db = TileDb::get_singleton();
    return tile_db ? tile_db->get_tile_info(tile_id) : nullptr;
}

static bool is_openable_tile(uint16_t tile_id, const TileInfo* tile) {
    TileDb* tile_db = TileDb::get_singleton();
    TagRegistry* tag_reg = TagRegistry::get_singleton();
    if (!tile_db || !tag_reg || !tile || tile->opens_to == 0) return false;

    const uint16_t can_open = tag_reg->get_tag_id("CAN_OPEN");
    return can_open != 0 && tile_db->has_tag(tile_id, can_open);
}

}

String TraversalRules::get_race_profile_id(const String& race_id) {
    RaceDb* race_db = RaceDb::get_singleton();
    const RaceInfo* race = race_db ? race_db->get_race_info(race_id) : nullptr;
    if (race && !race->traversal_profile.is_empty()) {
        return race->traversal_profile;
    }
    return "walker";
}

String TraversalRules::get_entity_profile_id(uint32_t entity_id, const EntityLedger& ledger) {
    const AnatomyData* anatomy = ledger.try_get_anatomy(entity_id);
    return anatomy ? get_race_profile_id(anatomy->race_id) : String("walker");
}

bool TraversalRules::can_enter(uint32_t entity_id, uint16_t tile_id, const EntityLedger& ledger) {
    const AnatomyData* anatomy = ledger.try_get_anatomy(entity_id);
    const TraversalProfileInfo* profile = get_profile(anatomy ? get_race_profile_id(anatomy->race_id) : String("walker"));
    if (!profile) return false;
    return can_profile_enter_internal(*profile, get_tile(tile_id), anatomy, true);
}

bool TraversalRules::can_race_enter(const String& race_id, uint16_t tile_id) {
    const TraversalProfileInfo* profile = get_profile(get_race_profile_id(race_id));
    if (!profile) return false;
    AnatomyData anatomy;
    Anatomy::init(anatomy, race_id);
    return can_profile_enter_internal(*profile, get_tile(tile_id), &anatomy, true);
}

bool TraversalRules::can_profile_enter(const String& profile_id, uint16_t tile_id) {
    const TraversalProfileInfo* profile = get_profile(profile_id);
    if (!profile) return false;
    return can_profile_enter_internal(*profile, get_tile(tile_id), nullptr, false);
}

bool TraversalRules::can_enter_or_open(uint32_t entity_id, uint16_t tile_id, const EntityLedger& ledger) {
    if (can_enter(entity_id, tile_id, ledger)) return true;

    const AnatomyData* anatomy = ledger.try_get_anatomy(entity_id);
    const TraversalProfileInfo* profile = get_profile(anatomy ? get_race_profile_id(anatomy->race_id) : String("walker"));
    const TileInfo* tile = get_tile(tile_id);
    if (!profile || !profile->can_open_doors || !is_openable_tile(tile_id, tile)) {
        return false;
    }

    return can_profile_enter_internal(*profile, tile, anatomy, true, true);
}

bool TraversalRules::can_race_enter_or_open(const String& race_id, uint16_t tile_id) {
    if (can_race_enter(race_id, tile_id)) return true;

    const TraversalProfileInfo* profile = get_profile(get_race_profile_id(race_id));
    const TileInfo* tile = get_tile(tile_id);
    if (!profile || !profile->can_open_doors || !is_openable_tile(tile_id, tile)) {
        return false;
    }

    AnatomyData anatomy;
    Anatomy::init(anatomy, race_id);
    return can_profile_enter_internal(*profile, tile, &anatomy, true, true);
}

bool TraversalRules::can_profile_enter_or_open(const String& profile_id, uint16_t tile_id) {
    if (can_profile_enter(profile_id, tile_id)) return true;

    const TraversalProfileInfo* profile = get_profile(profile_id);
    const TileInfo* tile = get_tile(tile_id);
    if (!profile || !profile->can_open_doors || !is_openable_tile(tile_id, tile)) {
        return false;
    }

    return can_profile_enter_internal(*profile, tile, nullptr, false, true);
}

bool TraversalRules::can_open_doors(uint32_t entity_id, const EntityLedger& ledger) {
    return can_profile_open_doors(get_entity_profile_id(entity_id, ledger));
}

bool TraversalRules::can_race_open_doors(const String& race_id) {
    return can_profile_open_doors(get_race_profile_id(race_id));
}

bool TraversalRules::can_profile_open_doors(const String& profile_id) {
    const TraversalProfileInfo* profile = get_profile(profile_id);
    return profile && profile->can_open_doors;
}
