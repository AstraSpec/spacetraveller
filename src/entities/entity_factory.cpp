#include "entity_factory.h"
#include "entity_ledger.h"
#include "entity_pool.h"
#include "world/world_bubble.h"
#include "world/turn_scheduler.h"
#include "world/entity_lifecycle.h"
#include "data/race_db.h"
#include "data/item_db.h"
#include "data/job_db.h"
#include "data/name_db.h"
#include "data/npc_role_db.h"
#include "core/id_registry.h"
#include "core/rng.h"
#include "core/faction.h"
#include "components/locomotion.h"
#include "components/perception.h"
#include "components/ai_controller.h"

#include <cstdlib>

using namespace godot;

uint32_t EntityFactory::create_player(const String& race_id, const Vector2i& pos,
                                      EntityLedger& ledger, WorldBubble& bubble, TurnScheduler& scheduler) {
    RaceDb* race_db = RaceDb::get_singleton();
    if (!race_db) return EntityPool::PLAYER_ID;

    Vector2i atlas = race_db->get_atlas_coords(race_id);
    uint32_t id = ledger.spawn_player(pos, atlas.x, atlas.y);

    LocomotionData& loco = ledger.locomotion_data[id];
    Locomotion::init(loco, 1.0f);

    ledger.combat_style[id] = "default";

    if (!EntityLifecycle::activate_entity(id, pos, 0.0f, ledger, bubble, scheduler)) {
        ledger.destroy_entity(id);
        return EntityPool::INVALID_ID;
    }
    return id;
}

uint32_t EntityFactory::create_npc(const String& race_id, const Vector2i& pos, int world_seed,
                                    EntityLedger& ledger, WorldBubble& bubble, TurnScheduler& scheduler) {
    SpawnOverrides overrides;
    return create_npc(race_id, pos, world_seed, ledger, bubble, scheduler, overrides, 0.0f);
}

uint32_t EntityFactory::create_npc(const String& race_id, const Vector2i& pos, int world_seed,
                                    EntityLedger& ledger, WorldBubble& bubble, TurnScheduler& scheduler,
                                    const SpawnOverrides& overrides,
                                    float p_initial_turn_time) {
    RaceDb* race_db = RaceDb::get_singleton();
    if (!race_db) return EntityPool::INVALID_ID;

    const RaceInfo* race = race_db->get_race_info(race_id);
    if (!race) return EntityPool::INVALID_ID;

    uint32_t id = ledger.spawn_entity(pos, race->atlas.x, race->atlas.y, race_id);
    if (id == EntityPool::INVALID_ID) return id;

    LocomotionData& loco = ledger.locomotion_data[id];
    Locomotion::init(loco, race->speed);

    ledger.combat_style[id] = race->combat_style;

    if (race_db->has_tag(race_id, "GENDER")) {
        Rng::Seeded rng = Rng::at(static_cast<uint32_t>(world_seed), pos, Rng::GENDER);
        String g = rng.chance(0.5f) ? "male" : "female";
        ledger.gender[id] = g;

        NameDb* name_db = NameDb::get_singleton();
        if (name_db) {
            Rng::Seeded name_rng = Rng::at(static_cast<uint32_t>(world_seed), pos, Rng::NAME);
            String full_name = name_db->generate(race_id, g, name_rng);
            if (!full_name.is_empty()) ledger.entity_name[id] = full_name;
        }
    }

    if (race_db->has_tag(race_id, "SAPIENT")) {
        ledger.init_relationship(id);
        Rng::Seeded profile_rng = Rng::at(static_cast<uint32_t>(world_seed), pos, Rng::LOOT);
        JobDb* job_db = JobDb::get_singleton();
        const JobInfo* job_info = nullptr;
        if (job_db) {
            job_info = overrides.job.is_empty() ? job_db->pick_weighted_job(profile_rng) : job_db->get_job_info(overrides.job);
        }

        String job = !overrides.job.is_empty() ? overrides.job : (job_info ? job_info->id : String("scavenger"));
        String dialogue_profile = !overrides.dialogue_profile.is_empty() ? overrides.dialogue_profile : (job_info ? job_info->dialogue_profile : String("scavenger"));
        ledger.init_social_profile(id, job, dialogue_profile);

        SocialProfileData& profile = ledger.social_profiles[id];
        if (!overrides.traits.is_empty()) {
            for (int i = 0; i < overrides.traits.size(); i++) {
                profile.traits.push_back(String(overrides.traits[i]));
            }
        } else if (job_info) {
            for (const String& trait : job_info->traits) {
                profile.traits.push_back(trait);
            }
        }

        if (!overrides.context_tags.is_empty()) {
            for (int i = 0; i < overrides.context_tags.size(); i++) {
                profile.context_tags.push_back(String(overrides.context_tags[i]));
            }
        } else if (job_info) {
            for (const String& tag : job_info->context_tags) {
                profile.context_tags.push_back(tag);
            }
        }

        if (profile.traits.is_empty()) {
            profile.traits.push_back(profile_rng.chance(0.5f) ? String("wary") : String("plainspoken"));
        }
    }

    AIData& ai = ledger.ai_data[id];
    bool sapient = race_db->has_tag(race_id, "SAPIENT");
    ai.role = overrides.role.is_empty()
        ? (sapient ? String("civilian") : String("monster"))
        : overrides.role.to_lower();

    String default_attitude = Faction::are_hostile(race->faction, String("human"))
        ? String("hostile")
        : String("neutral");
    String default_ai_state = "wander";

    NpcRoleDb* role_db = NpcRoleDb::get_singleton();
    if (role_db) {
        const NpcRoleInfo* role_info = role_db->get_role_info(ai.role);
        if (role_info) {
            if (!role_info->default_attitude.is_empty()) {
                default_attitude = role_info->default_attitude;
            }
            if (!role_info->default_ai_state.is_empty()) {
                default_ai_state = role_info->default_ai_state;
            }
            if (sapient && overrides.dialogue_profile.is_empty() && !role_info->default_dialogue_profile.is_empty()) {
                SocialProfileData& profile = ledger.social_profiles[id];
                profile.dialogue_profile = role_info->default_dialogue_profile;
            }
        }
    }

    ai.attitude = overrides.attitude.is_empty() ? default_attitude : overrides.attitude.to_lower();
    ai.state = AIController::state_from_string(
        overrides.ai_state.is_empty() ? default_ai_state : overrides.ai_state,
        AIState::WANDER
    );
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
            Rng::Seeded loot_rng = Rng::at(static_cast<uint32_t>(world_seed), pos, Rng::SPAWN_LOOT);
            int rand_idx = loot_rng.range(0, item_ids.size() - 1);
            String random_item = item_ids[rand_idx];
            ledger.add_inventory_item(id, random_item, 1);
        }
    }

    if (!EntityLifecycle::activate_entity(id, pos, p_initial_turn_time, ledger, bubble, scheduler)) {
        ledger.destroy_entity(id);
        return EntityPool::INVALID_ID;
    }
    return id;
}
