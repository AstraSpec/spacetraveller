#include "entity_factory.h"
#include "entity_ledger.h"
#include "entity_pool.h"
#include "entity_tracker.h"
#include "world/world_bubble.h"
#include "world/turn_scheduler.h"
#include "world/entity_lifecycle.h"
#include "data/race_db.h"
#include "data/job_db.h"
#include "data/name_db.h"
#include "core/rng.h"
#include "components/locomotion.h"
#include "components/ai_controller.h"

#include <cstdlib>
#include <algorithm>

using namespace godot;

namespace {
constexpr uint16_t PLAYER_ATLAS_X = 104;
constexpr uint16_t PLAYER_ATLAS_Y = 29;
}

uint32_t EntityFactory::create_player(const String& race_id, const Vector2i& pos,
                                      EntityLedger& ledger, EntityTracker& tracker, WorldBubble& bubble, TurnScheduler& scheduler) {
    RaceDb* race_db = RaceDb::get_singleton();
    if (!race_db) return EntityPool::PLAYER_ID;

    uint32_t id = ledger.spawn_player(pos, PLAYER_ATLAS_X, PLAYER_ATLAS_Y);

    LocomotionData& loco = ledger.locomotion_data[id];
    Locomotion::init(loco, 1.0f);

    ledger.combat_style[id] = "default";

    if (!EntityLifecycle::activate_entity(id, pos, 0.0f, ledger, tracker, bubble, scheduler)) {
        ledger.destroy_entity(id);
        return EntityPool::INVALID_ID;
    }
    return id;
}

uint32_t EntityFactory::create_npc(const String& race_id, const Vector2i& pos, int world_seed,
                                    EntityLedger& ledger, EntityTracker& tracker, WorldBubble& bubble, TurnScheduler& scheduler) {
    SpawnOverrides overrides;
    return create_npc(race_id, pos, world_seed, ledger, tracker, bubble, scheduler, overrides, 0.0f);
}

uint32_t EntityFactory::create_npc(const String& race_id, const Vector2i& pos, int world_seed,
                                    EntityLedger& ledger, EntityTracker& tracker, WorldBubble& bubble, TurnScheduler& scheduler,
                                    const SpawnOverrides& overrides,
                                    float p_initial_turn_time) {
    RaceDb* race_db = RaceDb::get_singleton();
    if (!race_db) return EntityPool::INVALID_ID;

    const RaceInfo* race = race_db->get_race_info(race_id);
    if (!race) return EntityPool::INVALID_ID;

    bool sapient = race_db->has_tag(race_id, "SAPIENT");
    Rng::Seeded profile_rng = Rng::at(static_cast<uint32_t>(world_seed), pos, Rng::LOOT);
    JobDb* job_db = JobDb::get_singleton();
    const JobInfo* job_info = nullptr;
    if (job_db && !overrides.job.is_empty()) {
        job_info = job_db->get_job_info(overrides.job);
    }
    String job = overrides.job;
    if (!job_info && job_db) {
        if (!job.is_empty()) job_info = job_db->get_job_info(job);
    }

    int atlas_x = race->atlas.x + (job_info ? job_info->atlas_offset : 0);
    if (atlas_x < 0) atlas_x = 0;

    uint32_t id = ledger.spawn_entity(pos, bubble.get_active_z(), static_cast<uint16_t>(atlas_x), race->atlas.y, race_id);
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

    if (sapient) {
        ledger.init_relationship(id);

        ledger.init_social_profile(id, job, overrides.dialogue_id);

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
    String faction = race->faction.is_empty() ? String("unaffiliated") : race->faction;
    String reaction_policy = race->reaction_policy;
    int reaction_radius = race->reaction_radius;
    String default_ai_state = "wander";

    if (job_info) {
        if (!job_info->faction.is_empty()) faction = job_info->faction;
        if (!job_info->reaction_policy.is_empty()) reaction_policy = job_info->reaction_policy;
        if (job_info->reaction_radius > 0) reaction_radius = job_info->reaction_radius;
        if (!job_info->default_ai_state.is_empty()) {
            default_ai_state = job_info->default_ai_state;
        }
    }

    if (!overrides.faction.is_empty()) faction = overrides.faction;
    if (!overrides.reaction_policy.is_empty()) reaction_policy = overrides.reaction_policy;
    if (overrides.reaction_radius > 0) reaction_radius = overrides.reaction_radius;
    ledger.allegiance_data[id].faction_id = faction.to_lower();
    ai.reaction_policy = AIController::reaction_policy_from_string(reaction_policy);
    ai.reaction_radius = std::max(1, reaction_radius);
    ai.state = AIController::state_from_string(
        overrides.ai_state.is_empty() ? default_ai_state : overrides.ai_state
    );
    ai.home_state = ai.state;
    ai.home_position = pos;
    ai.wander_radius = 4.0f;
    ai.calm_scan_countdown = 0;

    if (!EntityLifecycle::activate_entity(id, pos, p_initial_turn_time, ledger, tracker, bubble, scheduler)) {
        ledger.destroy_entity(id);
        return EntityPool::INVALID_ID;
    }
    return id;
}
