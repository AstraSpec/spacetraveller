#include "sim/simulation_director.h"

#include "sim/npc_turn_processor.h"
#include "path/path_request.h"
#include "path/path_result.h"
#include "data/tile_db.h"
#include "data/race_db.h"
#include "data/faction_db.h"
#include "data/style_db.h"
#include "data/loot_db.h"
#include "core/world_coords.h"
#include "core/id_registry.h"
#include "core/tag_registry.h"
#include "core/faction.h"
#include "world/entity_lifecycle.h"
#include "world/point_of_interest_registry.h"
#include "world/city_population_director.h"
#include "entities/entity_tracker.h"
#include "world/traversal_rules.h"
#include "components/action_planner.h"
#include "components/action_resolver.h"
#include "components/ai_controller.h"
#include "components/combat_resolver.h"
#include "components/perception.h"
#include "components/locomotion.h"
#include "components/stamina.h"
#include "components/effects.h"
#include "components/equipment.h"
#include "components/anatomy.h"
#include "components/physiology.h"
#include "data/body_part_db.h"
#include "data/recipe_db.h"
#include "data/item_db.h"
#include "data/tool_quality_db.h"
#include "components/inventory.h"
#include "components/activity.h"
#include "core/string_hasher.h"

#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector3i.hpp>
#include <cmath>
#include <vector>
#include <unordered_map>

using namespace godot;

namespace {
constexpr int ACTIVITY_EVENT_SAFETY_BUDGET = 512;
}

void SimulationDirector::configure(const SimulationDirectorDeps& deps) {
    d = deps;
}

const RaceInfo* SimulationDirector::get_race_info(uint32_t entity_id) const {
    const AnatomyData* anatomy = d.ledger->try_get_anatomy(entity_id);
    if (!anatomy) return nullptr;
    RaceDb* race_db = RaceDb::get_singleton();
    if (!race_db) return nullptr;
    return race_db->get_race_info(anatomy->race_id);
}

String SimulationDirector::entity_faction(uint32_t entity_id) const {
    const AllegianceData* allegiance = d.ledger->try_get_allegiance(entity_id);
    return allegiance ? allegiance->faction_id : String("unaffiliated");
}

bool SimulationDirector::entity_is_hostile_to(uint32_t entity_id, uint32_t target_id) const {
    if (entity_id == target_id) return false;
    if (!d.ledger->is_alive(entity_id) || !d.ledger->is_alive(target_id)) return false;

    const AllegianceData* allegiance = d.ledger->try_get_allegiance(entity_id);
    if (allegiance) {
        auto personal = allegiance->personal_relations.find(target_id);
        if (personal != allegiance->personal_relations.end()) {
            return personal->second == EntityRelation::HOSTILE;
        }
    }
    FactionDb* factions = FactionDb::get_singleton();
    return factions && factions->get_relation_value(entity_faction(entity_id), entity_faction(target_id)) == FactionRelation::HOSTILE;
}

bool SimulationDirector::can_interact_with_entity(uint32_t entity_id) const {
    if (entity_id == d.player_entity_id || !d.ledger->is_alive(entity_id) || !d.ledger->is_sapient(entity_id)) {
        return false;
    }
    if (entity_is_hostile_to(d.player_entity_id, entity_id) || entity_is_hostile_to(entity_id, d.player_entity_id)) {
        return false;
    }
    return true;
}

bool SimulationDirector::set_entity_relation(uint32_t entity_id, uint32_t target_id, const String& relation) {
    if (entity_id == target_id || !d.ledger->get_entity_pool().contains(entity_id) ||
        !d.ledger->get_entity_pool().contains(target_id)) {
        return false;
    }
    AllegianceData* allegiance = d.ledger->try_get_allegiance(entity_id);
    if (!allegiance) return false;

    const EntityRelation value = Allegiance::relation_from_string(relation);
    if (value == EntityRelation::NEUTRAL) {
        allegiance->personal_relations.erase(target_id);
    } else {
        allegiance->personal_relations[target_id] = value;
    }
    return true;
}

String SimulationDirector::get_entity_relation(uint32_t entity_id, uint32_t target_id) const {
    const AllegianceData* allegiance = d.ledger->try_get_allegiance(entity_id);
    if (allegiance) {
        auto personal = allegiance->personal_relations.find(target_id);
        if (personal != allegiance->personal_relations.end()) return Allegiance::relation_to_string(personal->second);
    }
    FactionDb* factions = FactionDb::get_singleton();
    const FactionRelation relation = factions
        ? factions->get_relation_value(entity_faction(entity_id), entity_faction(target_id))
        : FactionRelation::NEUTRAL;
    return Faction::relation_to_attitude_string(relation);
}

bool SimulationDirector::start_entity_follow(uint32_t entity_id) {
    if (entity_id == d.player_entity_id ||
        !d.ledger->is_alive(entity_id) ||
        !d.ledger->is_alive(d.player_entity_id) ||
        !d.ledger->is_sapient(entity_id)) {
        return false;
    }

    AIData* ai = d.ledger->try_get_ai(entity_id);
    if (!ai) return false;

    // Keep the relationship and behavior change together so the UI cannot
    // leave an NPC friendly without also assigning its follow order.
    AllegianceData* allegiance = d.ledger->try_get_allegiance(entity_id);
    if (!allegiance) return false;
    if (d.poi_registry) d.poi_registry->release_for_entity(entity_id);
    AIController::reset_routine(*ai, true);
    allegiance->faction_id = "player";
    allegiance->personal_relations[d.player_entity_id] = EntityRelation::FRIENDLY;
    ai->state = AIState::FOLLOW;
    ai->home_state = AIState::FOLLOW;
    ai->target_entity_id = d.player_entity_id;
    ai->follow_leader_id = d.player_entity_id;
    ai->blocked_move_count = 0;
    ai->path_retry_countdown = 0;
    ai->wait_turns = 0;
    ai->forced_reaction = false;

    if (LocomotionData* loco = d.ledger->try_get_locomotion(entity_id)) {
        Locomotion::clear_path(*loco);
    }
    return true;
}

bool SimulationDirector::set_entity_behavior(uint32_t entity_id, const String& state, uint32_t target_id) {
    if (!AIController::is_valid_state_name(state)) return false;
    AIData* ai = d.ledger->try_get_ai(entity_id);
    if (!ai) return false;

    const AIState next_state = AIController::state_from_string(state);
    const bool needs_target = next_state == AIState::COMBAT || next_state == AIState::FOLLOW || next_state == AIState::FLEE;
    if (needs_target) {
        if (target_id == EntityPool::INVALID_ID || target_id == entity_id ||
            !d.ledger->is_alive(target_id)) {
            return false;
        }
    }
    if (d.poi_registry) d.poi_registry->release_for_entity(entity_id);
    AIController::reset_routine(*ai, true);
    ai->target_entity_id = needs_target ? target_id : EntityPool::INVALID_ID;
    if (next_state == AIState::FOLLOW) {
        ai->follow_leader_id = target_id;
    } else if (next_state != AIState::COMBAT && next_state != AIState::FLEE) {
        ai->follow_leader_id = EntityPool::INVALID_ID;
    }
    ai->state = next_state;
    ai->forced_reaction = next_state == AIState::COMBAT || next_state == AIState::FLEE;
    if (next_state == AIState::COMBAT || next_state == AIState::FLEE) {
        if (const Entity* target = d.ledger->get_entity_pool().get_entity(target_id)) {
            ai->last_known_target_position = Vector2i(target->x, target->y);
            ai->has_last_known_target_position = true;
            ai->lost_target_turns = 0;
        }
    }
    if (next_state != AIState::COMBAT && next_state != AIState::FLEE) {
        ai->home_state = next_state;
        if (const Entity* entity = d.ledger->get_entity_pool().get_entity(entity_id)) {
            ai->home_position = Vector2i(entity->x, entity->y);
        }
    }
    ai->blocked_move_count = 0;
    ai->path_retry_countdown = 0;
    ai->wait_turns = 0;
    if (LocomotionData* loco = d.ledger->try_get_locomotion(entity_id)) {
        Locomotion::clear_path(*loco);
    }
    return true;
}

String SimulationDirector::get_entity_behavior_state(uint32_t entity_id) const {
    const AIData* ai = d.ledger->try_get_ai(entity_id);
    return ai ? AIController::state_to_string(ai->state) : String();
}

uint32_t SimulationDirector::get_entity_behavior_target(uint32_t entity_id) const {
    const AIData* ai = d.ledger->try_get_ai(entity_id);
    return ai ? ai->target_entity_id : EntityPool::INVALID_ID;
}

Array SimulationDirector::get_player_attack_options(uint32_t target_id) {
    Array result;
    Entity* player =
        d.ledger->get_entity_pool().get_entity(d.player_entity_id);
    if (!player ||
        !settle_entity_time(d.player_entity_id, player->next_turn_time) ||
        !settle_entity_time(target_id, player->next_turn_time)) {
        return result;
    }

    AnatomyData* attacker_anatomy = d.ledger->try_get_anatomy(d.player_entity_id);
    AnatomyData* defender_anatomy = d.ledger->try_get_anatomy(target_id);
    HealthData* defender_health = d.ledger->try_get_health(target_id);
    EquipmentData* attacker_equipment = d.ledger->try_get_equipment(d.player_entity_id);
    ClothingData* defender_clothing = d.ledger->try_get_clothing(target_id);
    StaminaData* attacker_stamina = d.ledger->try_get_stamina(d.player_entity_id);
    PhysiologyData* attacker_physiology =
        d.ledger->try_get_physiology(d.player_entity_id);
    PhysiologyData* defender_physiology =
        d.ledger->try_get_physiology(target_id);
    const EffectsData* defender_effects =
        d.ledger->try_get_effects(target_id);
    if (!attacker_anatomy || !defender_anatomy || !defender_health ||
        !attacker_equipment || !attacker_stamina || !defender_health->alive) {
        return result;
    }

    const StyleInfo* style = nullptr;
    const String* style_id = d.ledger->try_get_combat_style(d.player_entity_id);
    if (style_id) {
        StyleDb* style_db = StyleDb::get_singleton();
        if (style_db) style = style_db->get_style_info(*style_id);
    }

    Rng::Seeded rng = combat_rng_for(d.player_entity_id, target_id);
    CombatContext ctx{
        *attacker_anatomy,
        *defender_anatomy,
        *defender_health,
        *attacker_equipment,
        defender_clothing,
        rng,
        entity_base_damage(d.player_entity_id),
        entity_combat_skill(d.player_entity_id),
        style,
        attacker_stamina,
        String(),
        -1,
        nullptr,
        nullptr,
        0.0f,
        attacker_physiology,
        defender_physiology,
        defender_effects,
        entity_effective_pain(d.player_entity_id)
    };
    return CombatResolver::get_attack_options(ctx);
}

Array SimulationDirector::get_entity_targetable_body_parts(uint32_t entity_id) const {
    const AnatomyData* anatomy = d.ledger->try_get_anatomy(entity_id);
    if (!anatomy) return Array();
    Dictionary functional = Anatomy::get_functional_list(*anatomy);
    return functional.get("parts", Array());
}

Array SimulationDirector::get_player_targetable_body_parts(
    uint32_t target_id,
    const String& attack_id
) {
    float base_accuracy = 0.0f;
    for (const Variant& option_value : get_player_attack_options(target_id)) {
        if (option_value.get_type() != Variant::DICTIONARY) continue;
        const Dictionary option = option_value;
        if (String(option.get("id", "")) == attack_id) {
            base_accuracy = static_cast<float>(static_cast<double>(
                option.get("accuracy", 0.0)));
            break;
        }
    }
    if (base_accuracy <= 0.0f) return Array();

    const AnatomyData* anatomy = d.ledger->try_get_anatomy(target_id);
    if (!anatomy) return Array();
    Array result;
    const Array parts = get_entity_targetable_body_parts(target_id);
    for (const Variant& part_value : parts) {
        if (part_value.get_type() != Variant::DICTIONARY) continue;
        Dictionary part = part_value;
        const int index = static_cast<int>(part.get("index", -1));
        const float penalty = Anatomy::get_targeting_penalty(*anatomy, index);
        const float targeted_accuracy = MAX(0.0f, base_accuracy - penalty);
        part["aim_penalty"] = penalty;
        part["base_accuracy"] = base_accuracy;
        part["targeted_accuracy"] = targeted_accuracy;
        part["time_multiplier"] = 1.0f + penalty;
        part["deviation_window"] = base_accuracy - targeted_accuracy;
        result.push_back(part);
    }
    return result;
}

float SimulationDirector::entity_base_damage(uint32_t entity_id) const {
    const RaceInfo* race = get_race_info(entity_id);
    return race ? race->base_damage : 10.0f;
}

float SimulationDirector::entity_combat_skill(uint32_t entity_id) const {
    (void)entity_id;
    return CombatTuning::DEFAULT_COMBAT_SKILL;
}

uint64_t SimulationDirector::entity_rng_salt(const Entity* entity, uint32_t entity_id) {
    uint64_t salt = static_cast<uint64_t>(entity_id) << 32;
    if (entity) {
        salt ^= Rng::mix64(static_cast<uint64_t>(entity->next_turn_time * 1000.0f));
    }
    return salt;
}

Rng::Seeded SimulationDirector::combat_rng_for(uint32_t attacker_id, uint32_t defender_id) const {
    const Entity* attacker = d.ledger->get_entity_pool().get_entity(attacker_id);
    Vector2i pos = attacker ? Vector2i(attacker->x, attacker->y) : Vector2i();
    uint64_t salt = entity_rng_salt(attacker, attacker_id) ^ static_cast<uint64_t>(defender_id);
    uint32_t seed = d.world_seed ? static_cast<uint32_t>(*d.world_seed) : 0;
    return Rng::at(seed, pos, Rng::COMBAT, salt);
}

Rng::Seeded SimulationDirector::action_rng_for(uint32_t entity_id, const Vector2i& target, Rng::Stream stream) const {
    const Entity* entity = d.ledger->get_entity_pool().get_entity(entity_id);
    uint64_t salt = entity_rng_salt(entity, entity_id);
    uint32_t seed = d.world_seed ? static_cast<uint32_t>(*d.world_seed) : 0;
    return Rng::at(seed, target, stream, salt);
}

CombatOutcome SimulationDirector::resolve_entity_attack(
    uint32_t attacker_id,
    uint32_t defender_id,
    const String& attack_id,
    int body_part_index
) {
    CombatOutcome outcome;

    Entity* attacker =
        d.ledger->get_entity_pool().get_entity(attacker_id);
    if (!attacker) return outcome;
    const float attack_time = attacker->next_turn_time;
    if (!settle_entity_time(attacker_id, attack_time) ||
        !settle_entity_time(defender_id, attack_time)) {
        return outcome;
    }

    AnatomyData* attacker_anatomy = d.ledger->try_get_anatomy(attacker_id);
    AnatomyData* defender_anatomy = d.ledger->try_get_anatomy(defender_id);
    HealthData* defender_health = d.ledger->try_get_health(defender_id);
    EquipmentData* attacker_equipment = d.ledger->try_get_equipment(attacker_id);
    EquipmentData* defender_equipment = d.ledger->try_get_equipment(defender_id);
    ClothingData* defender_clothing = d.ledger->try_get_clothing(defender_id);
    StaminaData* attacker_stamina = d.ledger->try_get_stamina(attacker_id);
    CombatStateData* defender_combat_state =
        d.ledger->try_get_combat_state(defender_id);
    PhysiologyData* attacker_physiology =
        d.ledger->try_get_physiology(attacker_id);
    PhysiologyData* defender_physiology =
        d.ledger->try_get_physiology(defender_id);
    const EffectsData* defender_effects =
        d.ledger->try_get_effects(defender_id);
    if (!attacker_anatomy || !defender_anatomy || !defender_health ||
        !attacker_equipment || !attacker_stamina || !defender_health->alive) {
        return outcome;
    }

    const StyleInfo* style = nullptr;
    const String* style_id = d.ledger->try_get_combat_style(attacker_id);
    if (style_id) {
        StyleDb* style_db = StyleDb::get_singleton();
        if (style_db) style = style_db->get_style_info(*style_id);
    }

    Rng::Seeded rng = combat_rng_for(attacker_id, defender_id);
    CombatContext ctx{
        *attacker_anatomy,
        *defender_anatomy,
        *defender_health,
        *attacker_equipment,
        defender_clothing,
        rng,
        entity_base_damage(attacker_id),
        entity_combat_skill(attacker_id),
        style,
        attacker_stamina,
        attack_id,
        body_part_index,
        defender_equipment,
        defender_combat_state,
        get_entity_effective_movement_speed(defender_id),
        attacker_physiology,
        defender_physiology,
        defender_effects,
        entity_effective_pain(attacker_id)
    };
    return CombatResolver::resolve_attack(ctx);
}

void SimulationDirector::handle_entity_death(uint32_t entity_id, const String& cause, uint32_t killer_id) {
    if (entity_id == d.player_entity_id) {
        d.scheduler->remove(entity_id);
        if (has_player_activity()) {
            cancel_player_activity("death", false);
        }
        d.sink->on_player_died(cause);
        return;
    }

    d.sink->on_entity_died(entity_id, cause);
    if (d.event_listener) {
        GameEvent e;
        e.type = GameEventType::ENTITY_KILLED;
        e.subject_id = killer_id;
        e.target_id = entity_id;
        if (auto* dead = d.ledger->get_entity_pool().get_entity(entity_id)) {
            e.position = Vector2i(dead->x, dead->y);
        }
        d.event_listener->on_game_event(e);
    }
    if (d.city_population) {
        d.city_population->release_for_entity(entity_id);
    }
    uint32_t seed = d.world_seed ? static_cast<uint32_t>(*d.world_seed) : 0;
    EntityLifecycle::despawn_entity(
        entity_id,
        *d.ledger,
        *d.tracker,
        *d.bubble,
        *d.scheduler,
        seed,
        true,
        d.poi_registry
    );
}

bool SimulationDirector::has_player_activity() const {
    if (!d.ledger) return false;
    const ActivityData* activity = d.ledger->try_get_activity(d.player_entity_id);
    return activity && Activity::is_active(*activity);
}

Dictionary SimulationDirector::get_player_activity() const {
    if (!d.ledger) return Dictionary();
    const ActivityData* activity = d.ledger->try_get_activity(d.player_entity_id);
    return activity ? Activity::to_dictionary(*activity) : Dictionary();
}

Dictionary SimulationDirector::get_player_crafting_status(
    const String& recipe_id,
    const Dictionary& context
) const {
    Dictionary status;
    status["craftable"] = false;
    status["station_requested"] = context.has("station_pos");
    status["station_valid"] = !context.has("station_pos");
    status["station_relevant"] = false;
    status["components"] = Array();
    status["tools"] = Array();
    status["missing_components"] = Array();
    status["missing_tools"] = Array();
    if (!d.ledger || !d.bubble) return status;

    RecipeDb* recipes = RecipeDb::get_singleton();
    ItemDb* items = ItemDb::get_singleton();
    TileDb* tiles = TileDb::get_singleton();
    IdRegistry* registry = IdRegistry::get_singleton();
    ToolQualityDb* quality_db = ToolQualityDb::get_singleton();
    const RecipeInfo* recipe = recipes ? recipes->get_info(recipe_id) : nullptr;
    const InventoryData* inventory = d.ledger->try_get_inventory(d.player_entity_id);
    const Entity* player = d.ledger->get_entity_pool().get_entity(d.player_entity_id);
    if (!recipe || !items || !tiles || !registry || !inventory || !player) {
        return status;
    }

    std::unordered_map<String, int, StringHasher> required;
    std::vector<String> component_order;
    Array component_statuses;
    Array missing_components;
    for (const RecipeComponent& entry : recipe->components) {
        if (required.find(entry.item_id) == required.end()) {
            component_order.push_back(entry.item_id);
        }
        required[entry.item_id] += entry.amount;
    }
    for (const String& item_id : component_order) {
        const int amount = required[item_id];
        const int available = Inventory::get_item_amount(
            *inventory, registry->get_id(item_id));
        Dictionary entry;
        entry["item_id"] = item_id;
        entry["required"] = amount;
        entry["available"] = available;
        entry["satisfied"] = available >= amount;
        component_statuses.push_back(entry);
        if (available < amount) missing_components.push_back(entry);
    }

    std::unordered_map<uint16_t, RarityTier> available_qualities;
    for (const InventoryItem& carried : inventory->items) {
        const ItemInfo* item = items->get_item_info(carried.id);
        const ToolProviderInfo* tool = items->get_tool_info(carried.id);
        if (!item || !tool || carried.amount <= 0) continue;
        for (uint16_t quality : tool->qualities) {
            auto found = available_qualities.find(quality);
            if (found == available_qualities.end() ||
                !rarity_meets(found->second, item->rarity)) {
                available_qualities[quality] = item->rarity;
            }
        }
    }

    std::vector<uint16_t> station_qualities;
    bool station_valid = !context.has("station_pos");
    if (context.has("station_pos") &&
        context["station_pos"].get_type() == Variant::VECTOR2I) {
        const Vector2i station_pos = context["station_pos"];
        const int distance = MAX(
            std::abs(station_pos.x - player->x),
            std::abs(station_pos.y - player->y));
        const uint16_t station_id = distance <= 1
            ? d.bubble->query_tile_id_at_z(
                station_pos.x, station_pos.y, player->z)
            : 0;
        const TileInfo* station = tiles->get_tile_info(station_id);
        const ToolProviderInfo* station_tool =
            tiles->get_tool_info(station_id);
        station_valid = station_id != 0 && station && station_tool;
        if (station_valid) {
            const String station_name = registry->get_string(station_id);
            status["station_id"] = station_name;
            status["station_name"] = tiles->get_tile_name(station_name);
            status["station_pos"] = station_pos;
            status["station_rarity"] = rarity_to_string(station->rarity);
            station_qualities = station_tool->qualities;
            for (uint16_t quality : station_tool->qualities) {
                auto found = available_qualities.find(quality);
                if (found == available_qualities.end() ||
                    !rarity_meets(found->second, station->rarity)) {
                    available_qualities[quality] = station->rarity;
                }
            }
        }
    }

    Array tool_statuses;
    Array missing_tools;
    bool station_relevant = false;
    for (const RecipeToolRequirement& requirement :
         recipe->tool_requirements) {
        const String quality = registry->get_string(requirement.quality_id);
        const auto available = available_qualities.find(requirement.quality_id);
        const bool satisfied = available != available_qualities.end() &&
            rarity_meets(available->second, requirement.minimum_rarity);
        const bool station_provides = std::find(
            station_qualities.begin(), station_qualities.end(),
            requirement.quality_id) != station_qualities.end();
        station_relevant = station_relevant || station_provides;

        Dictionary entry;
        entry["quality"] = quality;
        entry["quality_name"] = quality_db
            ? quality_db->get_display_name(quality)
            : quality.capitalize();
        entry["required_rarity"] =
            rarity_to_string(requirement.minimum_rarity);
        entry["best_rarity"] = available != available_qualities.end()
            ? rarity_to_string(available->second)
            : String();
        entry["satisfied"] = satisfied;
        entry["station_provided"] = station_provides;
        tool_statuses.push_back(entry);
        if (!satisfied) missing_tools.push_back(entry);
    }

    status["station_valid"] = station_valid;
    status["station_relevant"] = station_relevant;
    status["components"] = component_statuses;
    status["tools"] = tool_statuses;
    status["missing_components"] = missing_components;
    status["missing_tools"] = missing_tools;
    status["craftable"] = station_valid && missing_components.is_empty() &&
        missing_tools.is_empty();
    return status;
}

bool SimulationDirector::validate_crafting_requirements(
    const String& recipe_id,
    const Dictionary& context
) const {
    return bool(get_player_crafting_status(recipe_id, context).get(
        "craftable", false));
}

void SimulationDirector::schedule_next_activity_work(ActivityData& activity) {
    Entity* player = d.ledger->get_entity_pool().get_entity(d.player_entity_id);
    if (!player || !Activity::is_running(activity)) return;
    const float remaining = MAX(0.0f, activity.total_time - activity.completed_time);
    activity.scheduled_work = MIN(1.0f, remaining);
    if (activity.scheduled_work <= 0.0f) return;
    player->condition_recovery_multiplier = 1.0f;
    player->next_turn_time = activity.simulation_time + activity.scheduled_work;
    d.scheduler->push(d.player_entity_id, player->next_turn_time);
}

bool SimulationDirector::start_player_crafting(
    const String& recipe_id,
    const Dictionary& context
) {
    if (!d.ledger || !d.scheduler || !d.sink || has_player_activity() ||
        !validate_crafting_requirements(recipe_id, context)) {
        return false;
    }
    const PhysiologyData* physiology =
        d.ledger->try_get_physiology(d.player_entity_id);
    if (physiology && physiology->downed) {
        d.sink->on_player_action_failed("downed");
        return false;
    }
    Entity* player = d.ledger->get_entity_pool().get_entity(d.player_entity_id);
    RecipeDb* recipes = RecipeDb::get_singleton();
    const RecipeInfo* recipe = recipes ? recipes->get_info(recipe_id) : nullptr;
    if (!player || !recipe ||
        !settle_entity_time(d.player_entity_id, player->next_turn_time)) {
        return false;
    }

    ActivityData& activity = d.ledger->ensure_activity(d.player_entity_id);
    activity = ActivityData();
    activity.type = "crafting";
    activity.subject_id = recipe_id;
    activity.total_time = recipe->time_seconds;
    activity.start_position = Vector3i(player->x, player->y, player->z);
    activity.simulation_time = player->next_turn_time;
    activity.last_refresh_time = activity.simulation_time;
    activity.next_refresh_time = activity.simulation_time + MIN(30.0f, activity.total_time);
    if (context.has("station_pos")) {
        const Dictionary status = get_player_crafting_status(recipe_id, context);
        activity.has_crafting_station = true;
        const Vector2i station_pos = status["station_pos"];
        activity.crafting_station_position = Vector3i(
            station_pos.x, station_pos.y, player->z);
        activity.crafting_station_tile_id = status["station_id"];
    }

    d.scheduler->remove(d.player_entity_id);
    schedule_next_activity_work(activity);
    d.sink->on_player_activity_started(Activity::to_dictionary(activity));
    return true;
}

void SimulationDirector::emit_activity_checkpoint(ActivityData& activity) {
    const float elapsed = MAX(0.0f, activity.simulation_time - activity.last_refresh_time);
    activity.last_refresh_time = activity.simulation_time;
    activity.next_refresh_time = MIN(
        activity.simulation_time + 30.0f,
        activity.simulation_time + MAX(0.0f, activity.total_time - activity.completed_time)
    );
    d.sink->on_player_activity_checkpoint(Activity::to_dictionary(activity, elapsed));
}

void SimulationDirector::emit_activity_interrupted(ActivityData& activity) {
    const float elapsed = MAX(0.0f, activity.simulation_time - activity.last_refresh_time);
    activity.last_refresh_time = activity.simulation_time;
    d.sink->on_player_activity_interrupted(Activity::to_dictionary(activity, elapsed));
}

bool SimulationDirector::request_activity_interruption(const String& interruption_id, uint32_t source_entity) {
    ActivityData* activity = d.ledger ? d.ledger->try_get_activity(d.player_entity_id) : nullptr;
    if (!activity || !Activity::is_running(*activity) || interruption_id.is_empty() ||
        Activity::ignores(*activity, interruption_id)) {
        return false;
    }
    activity->state = "interrupted";
    activity->pending_interruption = interruption_id;
    activity->pending_source_entity = source_entity;
    if (!processing_activity_batch) emit_activity_interrupted(*activity);
    return true;
}

void SimulationDirector::cancel_player_activity(const String& reason, bool restore_menu) {
    ActivityData* activity = d.ledger ? d.ledger->try_get_activity(d.player_entity_id) : nullptr;
    if (!activity || !Activity::is_active(*activity)) return;

    const float elapsed = MAX(0.0f, activity->simulation_time - activity->last_refresh_time);
    Dictionary payload = Activity::to_dictionary(*activity, elapsed);
    payload["reason"] = reason;
    payload["restore_menu"] = restore_menu;
    d.scheduler->remove(d.player_entity_id);
    if (Entity* player = d.ledger->get_entity_pool().get_entity(d.player_entity_id)) {
        player->next_turn_time = activity->simulation_time;
    }
    d.ledger->activity_data.erase(d.player_entity_id);
    d.sink->on_player_activity_cancelled(payload);
}

Dictionary SimulationDirector::crafting_context_from_activity(
    const ActivityData& activity
) const {
    Dictionary context;
    if (activity.has_crafting_station) {
        context["station_pos"] = Vector2i(
            activity.crafting_station_position.x,
            activity.crafting_station_position.y);
    }
    return context;
}

bool SimulationDirector::crafting_station_matches(
    const ActivityData& activity
) const {
    if (!activity.has_crafting_station) return true;
    if (!d.bubble) return false;
    IdRegistry* registry = IdRegistry::get_singleton();
    const uint16_t tile_id = d.bubble->query_tile_id_at_z(
        activity.crafting_station_position.x,
        activity.crafting_station_position.y,
        activity.crafting_station_position.z);
    return registry && registry->get_string(tile_id) ==
        activity.crafting_station_tile_id;
}

bool SimulationDirector::complete_crafting(
    const String& recipe_id,
    const Dictionary& context,
    Dictionary& completion
) {
    if (!validate_crafting_requirements(recipe_id, context)) return false;
    RecipeDb* recipes = RecipeDb::get_singleton();
    const RecipeInfo* recipe = recipes ? recipes->get_info(recipe_id) : nullptr;
    InventoryData* inventory = d.ledger->try_get_inventory(d.player_entity_id);
    Entity* player = d.ledger->get_entity_pool().get_entity(d.player_entity_id);
    if (!recipe || !inventory || !player) return false;

    std::unordered_map<String, int, StringHasher> required;
    for (const RecipeComponent& entry : recipe->components) required[entry.item_id] += entry.amount;
    for (const auto& pair : required) {
        if (!Inventory::remove_item_by_string(*inventory, pair.first, pair.second)) return false;
    }

    Array results;
    Array overflow;
    IdRegistry* ids = IdRegistry::get_singleton();
    for (const RecipeResult& entry : recipe->results) {
        Dictionary result;
        result["item_id"] = entry.item_id;
        result["amount"] = entry.amount;
        results.push_back(result);
        if (!Inventory::add_item_by_string(*inventory, entry.item_id, entry.amount)) {
            const uint16_t item_id = ids ? ids->get_id(entry.item_id) : 0;
            if (item_id != 0) {
                d.bubble->drop_item(Vector2i(player->x, player->y), item_id, entry.amount);
                overflow.push_back(result);
            }
        }
    }
    completion["results"] = results;
    completion["overflow"] = overflow;
    return true;
}

void SimulationDirector::complete_player_activity() {
    ActivityData* activity = d.ledger ? d.ledger->try_get_activity(d.player_entity_id) : nullptr;
    if (!activity || !Activity::is_active(*activity)) return;

    Dictionary completion;
    if (activity->type == "crafting" && !crafting_station_matches(*activity)) {
        cancel_player_activity("environment_changed");
        return;
    }
    const Dictionary context = crafting_context_from_activity(*activity);
    bool success = activity->type == "crafting" && complete_crafting(
        activity->subject_id, context, completion);
    if (!success) {
        cancel_player_activity("requirements_missing");
        return;
    }

    const float elapsed = MAX(0.0f, activity->simulation_time - activity->last_refresh_time);
    Dictionary payload = Activity::to_dictionary(*activity, elapsed);
    payload.merge(completion, true);
    d.scheduler->remove(d.player_entity_id);
    if (Entity* player = d.ledger->get_entity_pool().get_entity(d.player_entity_id)) {
        player->next_turn_time = activity->simulation_time;
    }
    d.ledger->activity_data.erase(d.player_entity_id);
    d.sink->on_player_activity_completed(payload);
}

void SimulationDirector::process_player_activity_batch() {
    ActivityData* activity = d.ledger ? d.ledger->try_get_activity(d.player_entity_id) : nullptr;
    if (!activity || !Activity::is_running(*activity) || !d.scheduler || !d.bubble) return;
    TileDb* tile_db = TileDb::get_singleton();
    if (!tile_db) return;

    processing_activity_batch = true;
    int processed = 0;
    while (processed < ACTIVITY_EVENT_SAFETY_BUDGET && Activity::is_running(*activity)) {
        const float event_time = d.scheduler->peek_time();
        if (!std::isfinite(event_time)) {
            processing_activity_batch = false;
            cancel_player_activity("scheduler_empty");
            return;
        }
        activity->simulation_time = event_time;
        const uint32_t entity_id = d.scheduler->pop();
        if (entity_id == EntityPool::INVALID_ID) break;
        ++processed;

        if (!settle_entity_time(entity_id, event_time)) {
            if (entity_id == d.player_entity_id) {
                processing_activity_batch = false;
                return;
            }
            continue;
        }

        if (entity_id == d.player_entity_id) {
            const float work = activity->scheduled_work;
            activity->scheduled_work = 0.0f;
            if (CombatStateData* combat_state =
                    d.ledger->try_get_combat_state(d.player_entity_id)) {
                CombatState::reset_defenses(*combat_state);
            }
            if (!has_player_activity()) {
                processing_activity_batch = false;
                return;
            }
            activity = d.ledger->try_get_activity(d.player_entity_id);
            activity->completed_time = MIN(activity->total_time, activity->completed_time + work);
            if (activity->completed_time >= activity->total_time) {
                processing_activity_batch = false;
                complete_player_activity();
                return;
            }
            schedule_next_activity_work(*activity);
        } else {
            NpcTurnProcessor::run_turn(
                entity_id,
                d.ledger->get_entity_pool(),
                *tile_db,
                *this
            );
        }

        if (!has_player_activity()) {
            processing_activity_batch = false;
            return;
        }
        activity = d.ledger->try_get_activity(d.player_entity_id);
        if (Activity::is_interrupted(*activity)) {
            processing_activity_batch = false;
            emit_activity_interrupted(*activity);
            return;
        }
        if (activity->simulation_time >= activity->next_refresh_time &&
            d.scheduler->peek_time() > activity->next_refresh_time) {
            processing_activity_batch = false;
            emit_activity_checkpoint(*activity);
            return;
        }
    }
    processing_activity_batch = false;
}

bool SimulationDirector::resolve_player_activity_interruption(const String& resolution) {
    ActivityData* activity = d.ledger ? d.ledger->try_get_activity(d.player_entity_id) : nullptr;
    if (!activity || !Activity::is_interrupted(*activity)) return false;
    if (resolution == "stop") {
        cancel_player_activity(activity->pending_interruption);
        return true;
    }
    if (resolution != "continue" && resolution != "ignore") return false;
    if (resolution == "ignore") Activity::ignore(*activity, activity->pending_interruption);
    activity->pending_interruption = "";
    activity->pending_source_entity = UINT32_MAX;
    activity->state = "running";
    return true;
}

void SimulationDirector::restore_player_activity() {
    ActivityData* activity = d.ledger ? d.ledger->try_get_activity(d.player_entity_id) : nullptr;
    if (!activity || !Activity::is_active(*activity)) return;
    if (activity->type == "crafting") {
        if (!crafting_station_matches(*activity)) {
            cancel_player_activity("environment_changed");
            return;
        }
        if (!validate_crafting_requirements(
                activity->subject_id,
                crafting_context_from_activity(*activity))) {
            cancel_player_activity("requirements_missing");
            return;
        }
    }
    Dictionary restored = Activity::to_dictionary(*activity);
    restored["restored"] = true;
    if (Activity::is_interrupted(*activity)) {
        d.sink->on_player_activity_started(restored);
        emit_activity_interrupted(*activity);
    } else {
        d.sink->on_player_activity_started(restored);
    }
}

bool SimulationDirector::finish_entity_action(
    uint32_t entity_id,
    float cost,
    float base_time,
    float stamina_cost,
    float stamina_recovery_multiplier
) {
    if (cost <= 0.0f) return false;
    if (!settle_entity_time(entity_id, base_time)) return true;

    StaminaData* stamina = d.ledger->try_get_stamina(entity_id);
    if (stamina && stamina_cost > 0.0f) {
        Stamina::drain(*stamina, stamina_cost);
    }

    if (CombatStateData* combat_state =
            d.ledger->try_get_combat_state(entity_id)) {
        CombatState::reset_defenses(*combat_state);
    }

    Entity* entity = d.ledger->get_entity_pool().get_entity(entity_id);
    if (!entity) return false;
    const HealthData* health = d.ledger->try_get_health(entity_id);
    if (health && !health->alive) {
        d.scheduler->remove(entity_id);
        return true;
    }

    float next_time = base_time + cost;
    entity->condition_time = base_time;
    entity->condition_recovery_multiplier =
        stamina_cost > 0.0f ? 0.0f : stamina_recovery_multiplier;
    entity->next_turn_time = next_time;
    d.scheduler->push(entity_id, next_time);
    return true;
}

float SimulationDirector::movement_action_cost(uint32_t entity_id, float base_cost, const LocomotionData& loco) const {
    if (base_cost <= 0.0f) return 0.0f;

    const float effective_speed = (loco.speed > 0.0f ? loco.speed : 1.0f)
        * Locomotion::movement_mode_speed_multiplier(loco.movement_mode)
        * entity_moving_capacity(entity_id);
    return effective_speed > 0.0f ? base_cost / effective_speed : 0.0f;
}

float SimulationDirector::entity_moving_capacity(uint32_t entity_id) const {
    if (!d.ledger) return 0.0f;

    float capacity = 1.0f;
    const AnatomyData* anatomy = d.ledger->try_get_anatomy(entity_id);
    if (anatomy) {
        capacity *= Anatomy::get_moving_capacity(*anatomy);
    }
    const StaminaData* stamina = d.ledger->try_get_stamina(entity_id);
    if (stamina) {
        capacity *= Stamina::moving_capacity_factor(*stamina);
    }
    const PhysiologyData* physiology =
        d.ledger->try_get_physiology(entity_id);
    if (physiology) {
        capacity *= CombatMath::pain_movement_multiplier(
            entity_effective_pain(entity_id));
        capacity *= CombatMath::consciousness_movement_multiplier(
            Physiology::get_consciousness_percent(*physiology));
    }
    return CLAMP(capacity, 0.0f, 1.0f);
}

float SimulationDirector::entity_physiology_action_speed(
    uint32_t entity_id
) const {
    if (!d.ledger) return 1.0f;
    const PhysiologyData* physiology =
        d.ledger->try_get_physiology(entity_id);
    if (!physiology) return 1.0f;
    return MAX(
        0.01f,
        CombatMath::pain_speed_multiplier(
            entity_effective_pain(entity_id)) *
        CombatMath::consciousness_speed_multiplier(
            Physiology::get_consciousness_percent(*physiology)));
}

float SimulationDirector::entity_pain_floor(uint32_t entity_id) const {
    if (!d.ledger) return 0.0f;
    const AnatomyData* anatomy =
        d.ledger->try_get_anatomy(entity_id);
    if (!anatomy) return 0.0f;
    const EffectsData* effects =
        d.ledger->try_get_effects(entity_id);
    return Anatomy::get_wound_pain_floor(
        *anatomy,
        effects ? Effects::total_bleed(*effects) : 0.0f);
}

float SimulationDirector::entity_effective_pain(uint32_t entity_id) const {
    if (!d.ledger) return 0.0f;
    const PhysiologyData* physiology =
        d.ledger->try_get_physiology(entity_id);
    return physiology
        ? MAX(physiology->pain, entity_pain_floor(entity_id))
        : 0.0f;
}

void SimulationDirector::reconcile_entity_physiology(
    uint32_t entity_id
) {
    if (!d.ledger) return;
    HealthData* health = d.ledger->try_get_health(entity_id);
    PhysiologyData* physiology =
        d.ledger->try_get_physiology(entity_id);
    if (!health || !health->alive || !physiology) return;
    const PhysiologyUpdate update = Physiology::reconcile(
        *physiology,
        Health::get_blood_percent(*health),
        entity_pain_floor(entity_id));
    if (update.reached_zero) {
        Health::kill(*health);
        handle_entity_death(entity_id, "consciousness", 0);
        return;
    }
    if (update.newly_downed) {
        force_entity_prone(entity_id, "downed");
        d.sink->on_effect_event(entity_id, "downed", "", "");
    } else if (update.recovered) {
        d.sink->on_effect_event(entity_id, "recovered", "", "");
    }
}

float SimulationDirector::get_entity_effective_movement_speed(uint32_t entity_id) const {
    if (!d.ledger) return 0.0f;
    const LocomotionData* loco = d.ledger->try_get_locomotion(entity_id);
    if (!loco) return 0.0f;
    const PhysiologyData* physiology =
        d.ledger->try_get_physiology(entity_id);
    if (physiology && physiology->downed) return 0.0f;

    float speed = (loco->speed > 0.0f ? loco->speed : 1.0f)
        * Locomotion::movement_mode_speed_multiplier(loco->movement_mode)
        * entity_moving_capacity(entity_id);
    return MAX(0.0f, speed);
}

String SimulationDirector::get_player_movement_mode() const {
    if (!d.ledger) return "walk";
    const LocomotionData* loco = d.ledger->try_get_locomotion(d.player_entity_id);
    return loco ? Locomotion::movement_mode_id(loco->movement_mode) : String("walk");
}

bool SimulationDirector::can_player_run() const {
    if (!d.ledger) return false;
    const StaminaData* stamina = d.ledger->try_get_stamina(d.player_entity_id);
    const PhysiologyData* physiology =
        d.ledger->try_get_physiology(d.player_entity_id);
    return stamina
        && (!physiology || !physiology->downed)
        && Stamina::get_percent(*stamina) > MovementTuning::RUN_MIN_STAMINA_PERCENT
        && stamina->current_stamina >= MovementTuning::RUN_STAMINA_PER_CARDINAL_STEP;
}

bool SimulationDirector::set_player_movement_mode(const String& mode_id, const String& reason) {
    if (!d.ledger) return false;
    LocomotionData* loco = d.ledger->try_get_locomotion(d.player_entity_id);
    if (!loco) return false;
    const PhysiologyData* physiology =
        d.ledger->try_get_physiology(d.player_entity_id);
    if (physiology && physiology->downed && mode_id != "prone") {
        if (d.sink) d.sink->on_player_action_failed("downed");
        return false;
    }

    MovementMode mode;
    if (!Locomotion::movement_mode_from_id(mode_id, mode)) return false;
    if (mode == MovementMode::RUN && loco->movement_mode != MovementMode::RUN && !can_player_run()) {
        return false;
    }
    if (loco->movement_mode == mode) return true;

    loco->movement_mode = mode;
    if (d.sink) {
        d.sink->on_player_movement_mode_changed(Locomotion::movement_mode_id(mode), reason);
    }
    return true;
}

bool SimulationDirector::toggle_player_run() {
    return get_player_movement_mode() == "run"
        ? set_player_movement_mode("walk", "toggle")
        : set_player_movement_mode("run", "toggle");
}

Array SimulationDirector::get_player_movement_mode_options() const {
    Array options;
    const String active = get_player_movement_mode();
    const PhysiologyData* physiology =
        d.ledger ? d.ledger->try_get_physiology(d.player_entity_id)
                 : nullptr;
    const bool downed = physiology && physiology->downed;
    const bool run_available = can_player_run() || active == "run";

    auto append_option = [&](MovementMode mode, const String& description, bool available) {
        Dictionary option;
        const String id = Locomotion::movement_mode_id(mode);
        option["id"] = id;
        option["name"] = Locomotion::movement_mode_name(mode);
        option["description"] = description;
        option["speed_multiplier"] = Locomotion::movement_mode_speed_multiplier(mode);
        option["active"] = id == active;
        option["available"] = available;
        options.push_back(option);
    };

    append_option(MovementMode::WALK, "", !downed);
    append_option(MovementMode::RUN, "", !downed && run_available);
    append_option(MovementMode::PRONE, "", true);
    return options;
}

void SimulationDirector::force_player_walk_if_exhausted() {
    if (get_player_movement_mode() != "run" || can_player_run()) return;
    set_player_movement_mode("walk", "exhausted");
}

void SimulationDirector::emit_movement_if_needed(uint32_t entity_id, const Vector2i& old_pos) {
    const Entity* entity = d.ledger->get_entity_pool().get_entity(entity_id);
    if (!entity || (entity->x == old_pos.x && entity->y == old_pos.y)) return;

    Vector2i new_pos(entity->x, entity->y);
    d.sink->on_entity_moved(entity_id, new_pos, entity_chunk(entity_id));
    if (d.event_listener) {
        GameEvent e;
        e.type = GameEventType::ENTITY_MOVED;
        e.subject_id = entity_id;
        e.position = new_pos;
        d.event_listener->on_game_event(e);
    }
}

bool SimulationDirector::submit_pickup(uint32_t entity_id, const Vector2i& pos, const String& item_id, int amount) {
    if (d.ledger == nullptr || d.bubble == nullptr || d.scheduler == nullptr) {
        return false;
    }

    Entity* entity = d.ledger->get_entity_pool().get_entity(entity_id);
    if (!entity) return false;

    float base_time = entity->next_turn_time;
    ActionResult result;

    if (entity_id == d.player_entity_id) {
        LocomotionData* loco = d.ledger->try_get_locomotion(entity_id);
        if (!loco) return false;

        Intent intent;
        intent.type = IntentType::PICKUP;
        intent.target = pos;
        intent.param = item_id;
        intent.amount = amount;
        result = resolve_player_action(intent, *entity, *loco);
    } else {
        Intent intent;
        intent.type = IntentType::PICKUP;
        intent.target = pos;
        intent.param = item_id;
        intent.amount = amount;
        result = resolve_pickup(entity_id, intent);
    }
    if (!result.success || result.cost <= 0.0f) return false;

    if (entity_id == d.player_entity_id) {
        finish_player_action(result, base_time, Vector2i(entity->x, entity->y), entity->z);
    } else {
        finish_entity_action(entity_id, result.cost, base_time);
    }

    return true;
}

void SimulationDirector::apply_attack_effects(uint32_t attacker_id, uint32_t defender_id, const CombatOutcome& atk) {
    if (!atk.hit) return;
    EffectsData& fx = d.ledger->ensure_effects(defender_id);

    if (!atk.effect_type.is_empty() && atk.effect_magnitude > 0.0f) {
        if (atk.effect_type == "bleed" && atk.hit_part_index >= 0) {
            Effects::add(fx, Effects::make_bleed(atk.hit_part_index, atk.effect_magnitude));
            d.sink->on_effect_event(defender_id, "bleed", "onset", atk.part_name);
        }
    }
    reconcile_entity_physiology(defender_id);
}

void SimulationDirector::force_entity_prone(
    uint32_t entity_id,
    const String& reason
) {
    LocomotionData* loco = d.ledger->try_get_locomotion(entity_id);
    if (!loco) return;
    const bool changed = loco->movement_mode != MovementMode::PRONE;
    loco->movement_mode = MovementMode::PRONE;
    Locomotion::clear_path(*loco);
    if (changed && entity_id == d.player_entity_id && d.sink) {
        d.sink->on_player_movement_mode_changed("prone", reason);
    }
    if (entity_id == d.player_entity_id && has_player_activity()) {
        cancel_player_activity(reason);
    }
}

void SimulationDirector::advance_entity_time(
    uint32_t entity_id,
    float dt,
    float physiology_recovery_multiplier
) {
    if (dt <= 0.0f) return;

    EffectsData* fx = d.ledger->try_get_effects(entity_id);
    const float bleed =
        fx ? Effects::total_bleed(*fx) : 0.0f;
    if (bleed > 0.0f) {
        HealthData* hp = d.ledger->try_get_health(entity_id);
        if (hp && hp->alive) {
            Health::drain_blood(
                *hp,
                Effects::bleed_blood_loss(*fx, dt));
            if (!hp->alive) {
                handle_entity_death(entity_id, "bleed", 0);
                return;
            }
        }
    }

    if (fx && !fx->effects.empty()) {
        std::vector<int> expired_bleeds;
        Effects::tick(*fx, dt, &expired_bleeds);

        for (int part_index : expired_bleeds) {
            String part_name = "";
            const AnatomyData* anatomy =
                d.ledger->try_get_anatomy(entity_id);
            if (anatomy) {
                BodyPartDb* bpd = BodyPartDb::get_singleton();
                if (bpd) {
                    part_name = bpd->get_body_part_name(
                        Anatomy::get_type_id(*anatomy, part_index));
                }
            }
            d.sink->on_effect_event(
                entity_id,
                "bleed",
                "stopped",
                part_name);
        }
    }

    HealthData* health = d.ledger->try_get_health(entity_id);
    PhysiologyData* physiology =
        d.ledger->try_get_physiology(entity_id);
    if (!health || !health->alive || !physiology) return;

    const PhysiologyUpdate update = Physiology::advance(
        *physiology,
        Health::get_blood_percent(*health),
        dt,
        physiology_recovery_multiplier,
        entity_pain_floor(entity_id));
    if (update.reached_zero) {
        Health::kill(*health);
        handle_entity_death(entity_id, "consciousness", 0);
        return;
    }
    if (update.newly_downed) {
        force_entity_prone(entity_id, "downed");
        d.sink->on_effect_event(entity_id, "downed", "", "");
    } else if (update.recovered) {
        d.sink->on_effect_event(entity_id, "recovered", "", "");
    }
}

bool SimulationDirector::settle_entity_time(
    uint32_t entity_id,
    float event_time
) {
    Entity* entity = d.ledger
        ? d.ledger->get_entity_pool().get_entity(entity_id)
        : nullptr;
    if (!entity) return false;

    const float elapsed = MAX(0.0f, event_time - entity->condition_time);
    if (elapsed > 0.0f) {
        const float recovery_multiplier =
            entity->condition_recovery_multiplier;
        entity->condition_time = event_time;
        if (StaminaData* stamina = d.ledger->try_get_stamina(entity_id)) {
            Stamina::recover_for_time(
                *stamina,
                elapsed,
                recovery_multiplier);
        }
        advance_entity_time(
            entity_id,
            elapsed,
            recovery_multiplier);
    }

    entity = d.ledger->get_entity_pool().get_entity(entity_id);
    if (!entity) return false;
    const HealthData* health = d.ledger->try_get_health(entity_id);
    return !health || health->alive;
}

bool SimulationDirector::plan_player_intent(Intent& intent) {
    if (intent.type != IntentType::MOVE) {
        return true;
    }

    const WorldBubble::CellEntity* occupant = d.bubble->get_entity_at(intent.target.x, intent.target.y);
    bool target_hostile = occupant ? entity_is_hostile_to(d.player_entity_id, occupant->entity_id)
                                   : false;
    if (occupant && !target_hostile) {
        target_hostile = entity_is_hostile_to(occupant->entity_id, d.player_entity_id);
    }
    const bool target_interactable = occupant && can_interact_with_entity(occupant->entity_id);
    bool target_auto_attack = target_hostile;
    if (occupant && !target_interactable) {
        const AIData* target_ai = d.ledger->try_get_ai(occupant->entity_id);
        target_auto_attack = target_auto_attack || (target_ai && target_ai->state == AIState::COMBAT);
    }
    ActionPlan plan = ActionPlanner::plan_player_intent(
        intent,
        *d.bubble,
        d.player_entity_id,
        target_auto_attack,
        target_interactable
    );
    if (plan.should_interact) {
        d.sink->on_interact_event(d.player_entity_id, plan.interact_target);
        return false;
    }

    intent = plan.intent;
    return true;
}

ActionResult SimulationDirector::resolve_player_attack(const Intent& intent) {
    const WorldBubble::CellEntity* occupant = d.bubble->get_entity_at(intent.target.x, intent.target.y);
    if (!occupant || occupant->entity_id == d.player_entity_id) {
        return ActionResult::make_failure(ActionFailure::INVALID_TARGET);
    }

    uint32_t defender_id = occupant->entity_id;
    if (!d.ledger->is_alive(defender_id)) {
        return ActionResult::make_failure(ActionFailure::INVALID_TARGET);
    }

    if (!intent.attack_id.is_empty()) {
        bool found_attack = false;
        for (const Variant& option_value : get_player_attack_options(defender_id)) {
            if (option_value.get_type() != Variant::DICTIONARY) continue;
            Dictionary option = option_value;
            if (String(option.get("id", "")) == intent.attack_id) {
                if (bool(option.get("disabled", false))) {
                    return ActionResult::make_failure(ActionFailure::EXHAUSTED);
                }
                found_attack = true;
                break;
            }
        }
        if (!found_attack || intent.attack_body_part < 0) {
            return ActionResult::make_failure(ActionFailure::INVALID_TARGET);
        }

        bool found_body_part = false;
        for (const Variant& part_value : get_entity_targetable_body_parts(defender_id)) {
            if (part_value.get_type() != Variant::DICTIONARY) continue;
            Dictionary part = part_value;
            if (static_cast<int>(part.get("index", -1)) == intent.attack_body_part) {
                found_body_part = true;
                break;
            }
        }
        if (!found_body_part) {
            return ActionResult::make_failure(ActionFailure::INVALID_TARGET);
        }
    }

    float cost = resolve_attack(
        d.player_entity_id,
        defender_id,
        true,
        intent.attack_id,
        intent.attack_body_part
    );
    if (cost <= 0.0f) {
        return ActionResult::make_failure(ActionFailure::INVALID_TARGET);
    }
    ActionResult result = ActionResult::make_success(cost);
    result.stamina_recovery_multiplier = 0.0f;
    return result;
}

ActionResult SimulationDirector::resolve_player_smash(const Intent& intent, Entity& entity, LocomotionData& loco) {
    uint16_t tile_numeric = d.bubble->query_tile_id(intent.target.x, intent.target.y);
    IdRegistry* reg = IdRegistry::get_singleton();
    String tile_id = reg ? reg->get_string(tile_numeric) : "";

    StaminaData* stamina = d.ledger->try_get_stamina(d.player_entity_id);
    if (stamina && !Stamina::can_afford(*stamina, StaminaTuning::SMASH_COST)) {
        d.sink->on_smash_event(d.player_entity_id, tile_id, "exhausted");
        return ActionResult::make_failure(ActionFailure::EXHAUSTED);
    }

    Rng::Seeded action_rng = action_rng_for(d.player_entity_id, intent.target, Rng::ACTION);
    if (action_rng.chance(ActionTuning::SMASH_FAIL_CHANCE)) {
        if (stamina) Stamina::drain(*stamina, StaminaTuning::SMASH_COST);
        d.sink->on_smash_event(d.player_entity_id, tile_id, "failed");
        ActionResult result = ActionResult::make_success(
            ActionCost::SMASH /
            entity_physiology_action_speed(d.player_entity_id));
        result.stamina_recovery_multiplier = 0.0f;
        return result;
    }

    ActionResult smash_result = ActionResolver::resolve(d.player_entity_id, intent, *d.bubble, entity, loco, d.ledger, d.tracker);
    if (smash_result.success && smash_result.cost > 0.0f) {
        smash_result.cost /=
            entity_physiology_action_speed(d.player_entity_id);
    }
    float cost = smash_result.cost;
    if (smash_result.success && cost > 0.0f) {
        if (stamina) Stamina::drain(*stamina, StaminaTuning::SMASH_COST);
        smash_result.stamina_recovery_multiplier = 0.0f;
        TileDb* tile_db_singleton = TileDb::get_singleton();
        const TileInfo* smashed_tile = tile_db_singleton ? tile_db_singleton->get_tile_info(tile_numeric) : nullptr;
        LootDb* loot_db = LootDb::get_singleton();
        if (smashed_tile && smashed_tile->smash_loot_table != 0 && loot_db) {
            uint32_t seed = d.world_seed ? static_cast<uint32_t>(*d.world_seed) : 0;
            Rng::Seeded loot_rng = Rng::at(
                seed,
                Vector3i(intent.target.x, intent.target.y, entity.z),
                Rng::TILE_LOOT
            );
            std::vector<LootStack> stacks;
            loot_db->roll_table(smashed_tile->smash_loot_table, loot_rng, stacks);
            for (const LootStack& stack : stacks) {
                if (stack.item_id != 0 && stack.amount > 0) {
                    d.bubble->drop_item(intent.target, stack.item_id, stack.amount);
                }
            }
        }
        d.sink->on_smash_event(d.player_entity_id, tile_id, "smashed");
    }

    return smash_result;
}

ActionResult SimulationDirector::resolve_player_basic_action(const Intent& intent, Entity& entity, LocomotionData& loco) {
    if ((intent.type == IntentType::MOVE || intent.type == IntentType::CHANGE_Z) &&
        entity_moving_capacity(d.player_entity_id) <= 0.0f) {
        return ActionResult::make_failure(ActionFailure::NO_LOCOMOTION);
    }

    if (intent.type == IntentType::MOVE && loco.movement_mode == MovementMode::RUN && !can_player_run()) {
        set_player_movement_mode("walk", "exhausted");
    }

    ActionResult result = ActionResolver::resolve(d.player_entity_id, intent, *d.bubble, entity, loco, d.ledger, d.tracker);
    if (!result.success) return result;

    if (intent.type == IntentType::MOVE) {
        const float base_step_cost = result.cost;
        result.cost = movement_action_cost(d.player_entity_id, result.cost, loco);
        if (loco.movement_mode == MovementMode::RUN) {
            result.stamina_cost =
                MovementTuning::RUN_STAMINA_PER_CARDINAL_STEP * base_step_cost;
            result.stamina_recovery_multiplier = 0.0f;
        } else if (loco.movement_mode == MovementMode::PRONE) {
            result.stamina_recovery_multiplier = 0.25f;
        } else {
            result.stamina_recovery_multiplier = 0.5f;
        }
    } else {
        if (result.cost > 0.0f) {
            result.cost /=
                entity_physiology_action_speed(d.player_entity_id);
        }
        if (intent.type == IntentType::CHANGE_Z) {
            result.stamina_recovery_multiplier = 0.5f;
        }
    }
    return result;
}

ActionResult SimulationDirector::resolve_player_pickup(const Intent& intent) {
    ActionResult result = resolve_pickup(d.player_entity_id, intent);
    if (result.success && result.cost > 0.0f) {
        result.cost /=
            entity_physiology_action_speed(d.player_entity_id);
    }
    return result;
}

ActionResult SimulationDirector::resolve_pickup(uint32_t entity_id, const Intent& intent) {
    Entity* entity = d.ledger->get_entity_pool().get_entity(entity_id);
    if (!entity) return ActionResult::make_failure(ActionFailure::MISSING_COMPONENT);

    int dx = abs(intent.target.x - entity->x);
    int dy = abs(intent.target.y - entity->y);
    if (dx > 1 || dy > 1) return ActionResult::make_failure(ActionFailure::INVALID_TARGET);

    InventoryData* inventory = d.ledger->try_get_inventory(entity_id);
    if (!inventory) return ActionResult::make_failure(ActionFailure::MISSING_COMPONENT);

    return ActionResolver::resolve_pickup(
        entity_id,
        intent.target,
        intent.param,
        intent.amount,
        *d.bubble,
        *inventory,
        d.event_listener
    );
}

ActionResult SimulationDirector::resolve_player_action(const Intent& intent, Entity& entity, LocomotionData& loco) {
    const PhysiologyData* physiology =
        d.ledger->try_get_physiology(d.player_entity_id);
    if (physiology && physiology->downed) {
        const bool allowed =
            intent.type == IntentType::NONE ||
            intent.type == IntentType::MOVE ||
            intent.type == IntentType::PICKUP ||
            intent.type == IntentType::OPEN ||
            intent.type == IntentType::CLOSE;
        if (!allowed) {
            return ActionResult::make_failure(ActionFailure::DOWNED);
        }
        if (intent.type == IntentType::MOVE) {
            loco.movement_mode = MovementMode::PRONE;
        }
    }
    switch (intent.type) {
        case IntentType::ATTACK:
            return resolve_player_attack(intent);
        case IntentType::SMASH:
            return resolve_player_smash(intent, entity, loco);
        case IntentType::PICKUP:
            return resolve_player_pickup(intent);
        default:
            return resolve_player_basic_action(intent, entity, loco);
    }
}

void SimulationDirector::emit_player_action_failure(ActionFailure failure) const {
    if (!d.sink) return;
    if (failure == ActionFailure::NO_LOCOMOTION) {
        d.sink->on_player_action_failed("no_locomotion");
    } else if (failure == ActionFailure::DOWNED) {
        d.sink->on_player_action_failed("downed");
    }
}

bool SimulationDirector::finish_player_action(const ActionResult& result, float base_time, const Vector2i& old_pos, int old_z) {
    if (!result.success || result.cost <= 0.0f) return false;
    if (!finish_entity_action(
            d.player_entity_id,
            result.cost,
            base_time,
            result.stamina_cost,
            result.stamina_recovery_multiplier)) {
        return false;
    }
    if (result.stamina_cost > 0.0f) {
        force_player_walk_if_exhausted();
    }

    emit_movement_if_needed(d.player_entity_id, old_pos);
    Entity* player = d.ledger->get_entity_pool().get_entity(d.player_entity_id);
    if (!player) return false;
    const HealthData* player_health =
        d.ledger->try_get_health(d.player_entity_id);
    if (player_health && !player_health->alive) return true;
    if (player->z != old_z) {
        d.bubble->set_active_z(player->z);
        d.bubble->rebuild_from_pool();
    }

    const float next_turn_time = player->next_turn_time;
    process_game_turn(next_turn_time);
    player_health = d.ledger->try_get_health(d.player_entity_id);
    if (player_health && !player_health->alive) return true;
    d.sink->on_player_action_resolved(
        d.player_entity_id,
        result.cost,
        next_turn_time);
    return true;
}

float SimulationDirector::execute_player_intent(Intent intent) {
    if (has_player_activity()) return 0.0f;
    Entity* entity = d.ledger->get_entity_pool().get_entity(d.player_entity_id);
    if (!entity) return 0.0f;

    HealthData* player_health = d.ledger->try_get_health(d.player_entity_id);
    if (player_health && !player_health->alive) {
        return 0.0f;
    }

    if (!plan_player_intent(intent)) return 0.0f;

    LocomotionData* loco = d.ledger->try_get_locomotion(d.player_entity_id);
    if (!loco) return 0.0f;
    float player_base_time = entity->next_turn_time;
    Vector2i old_pos(entity->x, entity->y);
    int old_z = entity->z;
    ActionResult result = resolve_player_action(intent, *entity, *loco);
    if (!result.success) emit_player_action_failure(result.failure);
    finish_player_action(result, player_base_time, old_pos, old_z);

    return result.success ? result.cost : 0.0f;
}

float SimulationDirector::submit_player_intent(int intent_type, int target_x, int target_y, const String& param) {
    if (d.ledger == nullptr || d.tracker == nullptr || d.bubble == nullptr || d.scheduler == nullptr || d.sink == nullptr) {
        return 0.0f;
    }

    Intent intent;
    intent.type = static_cast<IntentType>(intent_type);
    intent.target = Vector2i(target_x, target_y);
    intent.param = param;
    return execute_player_intent(intent);
}

float SimulationDirector::submit_player_targeted_attack(
    uint32_t target_id,
    const String& attack_id,
    int body_part_index
) {
    if (d.ledger == nullptr || d.tracker == nullptr || d.bubble == nullptr || d.scheduler == nullptr || d.sink == nullptr) {
        return 0.0f;
    }

    Entity* player = d.ledger->get_entity_pool().get_entity(d.player_entity_id);
    Entity* target = d.ledger->get_entity_pool().get_entity(target_id);
    if (!player || !target || target_id == d.player_entity_id || !d.ledger->is_alive(target_id)) {
        return 0.0f;
    }

    const int dx = abs(target->x - player->x);
    const int dy = abs(target->y - player->y);
    if (target->z != player->z || dx > 1 || dy > 1 || (dx == 0 && dy == 0)) {
        return 0.0f;
    }

    const WorldBubble::CellEntity* occupant = d.bubble->get_entity_at(target->x, target->y);
    if (!occupant || occupant->entity_id != target_id) {
        return 0.0f;
    }

    Intent intent;
    intent.type = IntentType::ATTACK;
    intent.target = Vector2i(target->x, target->y);
    intent.attack_id = attack_id;
    intent.attack_body_part = body_part_index;
    return execute_player_intent(intent);
}

float SimulationDirector::submit_player_change_z(int delta) {
    if (d.ledger == nullptr || d.tracker == nullptr || d.bubble == nullptr || d.scheduler == nullptr || d.sink == nullptr) {
        return 0.0f;
    }

    if (has_player_activity()) return 0.0f;
    Entity* entity = d.ledger->get_entity_pool().get_entity(d.player_entity_id);
    if (!entity) return 0.0f;

    Intent intent;
    intent.type = IntentType::CHANGE_Z;
    intent.target = Vector2i(entity->x, entity->y);
    intent.amount = delta;

    HealthData* player_health = d.ledger->try_get_health(d.player_entity_id);
    if (player_health && !player_health->alive) {
        return 0.0f;
    }

    LocomotionData* loco = d.ledger->try_get_locomotion(d.player_entity_id);
    if (!loco) return 0.0f;
    float player_base_time = entity->next_turn_time;
    Vector2i old_pos(entity->x, entity->y);
    int old_z = entity->z;
    ActionResult result = resolve_player_action(intent, *entity, *loco);
    if (!result.success) emit_player_action_failure(result.failure);
    finish_player_action(result, player_base_time, old_pos, old_z);

    return result.success ? result.cost : 0.0f;
}

void SimulationDirector::notify_attack(uint32_t attacker_id, uint32_t defender_id) {
    const Entity* attacker = d.ledger->get_entity_pool().get_entity(attacker_id);
    const Entity* defender = d.ledger->get_entity_pool().get_entity(defender_id);
    if (!attacker || !defender || attacker->z != defender->z) return;
    set_entity_relation(defender_id, attacker_id, "hostile");

    std::vector<uint32_t> observers;
    const int radius = d.bubble->get_active_radius();
    if (d.tracker) {
        d.tracker->query_rect(
            Vector2i(defender->x - radius, defender->y - radius),
            Vector2i(defender->x + radius, defender->y + radius),
            observers,
            defender->z
        );
    } else {
        observers = d.ledger->get_entity_pool().get_live_ids();
    }

    TileDb* tile_db = TileDb::get_singleton();
    for (uint32_t observer_id : observers) {
        if (observer_id == attacker_id || !d.ledger->is_alive(observer_id)) continue;
        AIData* ai = d.ledger->try_get_ai(observer_id);
        Entity* observer = d.ledger->get_entity_pool().get_entity(observer_id);
        if (!ai || !observer || observer->z != attacker->z) continue;
        const bool is_defender = observer_id == defender_id;
        if (!is_defender && get_entity_relation(observer_id, defender_id) != "friendly") continue;
        const int distance = std::max(abs(observer->x - attacker->x), abs(observer->y - attacker->y));
        int reaction_radius = ai->reaction_radius;
        const AnatomyData* observer_anatomy = d.ledger->try_get_anatomy(observer_id);
        if (observer_anatomy) {
            reaction_radius = MAX(1, static_cast<int>(
                std::round(reaction_radius * Anatomy::get_perception_capacity(*observer_anatomy))));
        }
        if (!is_defender && distance > reaction_radius) continue;
        if (!is_defender && tile_db &&
            (!Perception::has_line_of_sight(observer->x, observer->y, attacker->x, attacker->y, *d.bubble, *tile_db) ||
             !Perception::has_line_of_sight(observer->x, observer->y, defender->x, defender->y, *d.bubble, *tile_db))) {
            continue;
        }

        set_entity_relation(observer_id, attacker_id, "hostile");
        if (ai->reaction_policy == ReactionPolicy::PASSIVE) continue;
        if (d.poi_registry) d.poi_registry->release_for_entity(observer_id);
        AIController::reset_routine(*ai, true);
        ai->target_entity_id = attacker_id;
        ai->last_known_target_position = Vector2i(attacker->x, attacker->y);
        ai->has_last_known_target_position = true;
        ai->lost_target_turns = 0;
        ai->state = ai->reaction_policy == ReactionPolicy::TIMID ? AIState::FLEE : AIState::COMBAT;
        ai->forced_reaction = false;
        ai->blocked_move_count = 0;
        ai->path_retry_countdown = 0;
        ai->wait_turns = 0;
        if (LocomotionData* loco = d.ledger->try_get_locomotion(observer_id)) Locomotion::clear_path(*loco);
    }
}

float SimulationDirector::resolve_attack(
    uint32_t attacker_id,
    uint32_t defender_id,
    bool is_player,
    const String& attack_id,
    int body_part_index
) {
    if (d.city_population) {
        if (attacker_id == d.player_entity_id) d.city_population->promote(defender_id);
        if (defender_id == d.player_entity_id) d.city_population->promote(attacker_id);
    }
    if (!d.ledger->is_alive(defender_id)) {
        return 1.0f;
    }

    CombatOutcome atk = resolve_entity_attack(
        attacker_id,
        defender_id,
        attack_id,
        body_part_index);
    if (!d.ledger->is_alive(defender_id)) {
        return ActionCost::ATTACK;
    }

    if (atk.invalid_selection) {
        return 0.0f;
    }
    if (atk.incapacitated) {
        d.sink->on_combat_event(
            attacker_id,
            defender_id,
            0.0f,
            "downed",
            atk.verb,
            "");
        return ActionCost::ATTACK;
    }

    notify_attack(attacker_id, defender_id);

    if (atk.no_limbs) {
        d.sink->on_combat_event(attacker_id, defender_id, 0.0f, "no_limbs", atk.verb, "");
        if (defender_id == d.player_entity_id) request_activity_interruption("attacked", attacker_id);
        return ActionCost::ATTACK;
    }

    if (atk.exhausted) {
        d.sink->on_combat_event(attacker_id, defender_id, 0.0f, "exhausted", atk.verb, "");
        if (defender_id == d.player_entity_id) request_activity_interruption("attacked", attacker_id);
        return ActionCost::ATTACK;
    }

    float cost =
        ActionCost::ATTACK /
        (atk.speed > 0.0f ? atk.speed : 1.0f) *
        atk.aim_time_multiplier;

    if (atk.newly_downed && !atk.killed) {
        force_entity_prone(defender_id, "downed");
    }

    String result_str;
    if (atk.dodged) result_str = "dodge";
    else if (!atk.hit) result_str = "miss";
    else {
        result_str = atk.quality_name;
        if (atk.killed) result_str += "_kill";
        else if (atk.newly_downed) result_str += "_down";
    }
    d.sink->on_combat_event(attacker_id, defender_id, atk.damage, result_str, atk.verb, atk.part_name);

    Entity* defender = d.ledger->get_entity_pool().get_entity(defender_id);
    IdRegistry* ids = IdRegistry::get_singleton();
    for (const String& item_id : atk.dropped_weapon_ids) {
        const uint16_t numeric_id = ids ? ids->get_id(item_id) : 0;
        if (defender && numeric_id != 0) {
            d.bubble->drop_item(
                Vector2i(defender->x, defender->y),
                numeric_id,
                1);
            d.sink->on_effect_event(
                defender_id,
                "drop_weapon",
                item_id,
                "");
        } else {
            d.ledger->add_inventory_item(defender_id, item_id, 1);
        }
    }
    if (atk.killed) {
        handle_entity_death(
            defender_id,
            atk.consciousness_death ? String("consciousness")
                                    : String("combat"),
            attacker_id);
    } else {
        apply_attack_effects(attacker_id, defender_id, atk);
        if (defender_id == d.player_entity_id) request_activity_interruption("attacked", attacker_id);
    }

    return cost;
}

bool SimulationDirector::finish_short_player_action() {
    Entity* player = d.ledger ? d.ledger->get_entity_pool().get_entity(d.player_entity_id) : nullptr;
    if (!player || has_player_activity()) return false;
    const Vector2i position(player->x, player->y);
    return finish_player_action(
        ActionResult::make_success(
            ActionCost::INTERACT /
            entity_physiology_action_speed(d.player_entity_id)),
        player->next_turn_time,
        position,
        player->z
    );
}

bool SimulationDirector::submit_player_drop(const String& item_id, int amount) {
    if (!d.ledger || !d.bubble || amount <= 0 || has_player_activity()) return false;
    InventoryData* inventory = d.ledger->try_get_inventory(d.player_entity_id);
    Entity* player = d.ledger->get_entity_pool().get_entity(d.player_entity_id);
    IdRegistry* ids = IdRegistry::get_singleton();
    const uint16_t id = ids ? ids->get_id(item_id) : 0;
    if (!inventory || !player || id == 0 || !Inventory::remove_item(*inventory, id, amount)) return false;

    d.bubble->drop_item(Vector2i(player->x, player->y), id, amount);
    if (finish_short_player_action()) return true;
    d.bubble->remove_item(Vector2i(player->x, player->y), id, amount);
    Inventory::add_item(*inventory, id, amount);
    return false;
}

bool SimulationDirector::submit_player_wield(const String& item_id) {
    if (!d.ledger || has_player_activity() ||
        d.ledger->get_inventory_item_amount(d.player_entity_id, item_id) <= 0) return false;
    const PhysiologyData* physiology =
        d.ledger->try_get_physiology(d.player_entity_id);
    if (physiology && physiology->downed) {
        if (d.sink) d.sink->on_player_action_failed("downed");
        return false;
    }
    EquipmentData* equipment = d.ledger->try_get_equipment(d.player_entity_id);
    if (!equipment) return false;
    String slot;
    if (!Equipment::is_slot_occupied(*equipment, Equipment::MAIN_HAND_SLOT)) {
        slot = Equipment::MAIN_HAND_SLOT;
    } else if (!Equipment::is_slot_occupied(*equipment, Equipment::OFF_HAND_SLOT)) {
        slot = Equipment::OFF_HAND_SLOT;
    } else {
        return false;
    }
    if (!d.ledger->wield_weapon(d.player_entity_id, slot, item_id)) return false;
    if (!d.ledger->remove_inventory_item(d.player_entity_id, item_id, 1)) {
        d.ledger->unwield_weapon(d.player_entity_id, slot);
        return false;
    }
    if (finish_short_player_action()) return true;
    d.ledger->add_inventory_item(d.player_entity_id, item_id, 1);
    d.ledger->unwield_weapon(d.player_entity_id, slot);
    return false;
}

bool SimulationDirector::submit_player_unwield(const String& slot_name) {
    if (!d.ledger || has_player_activity()) return false;
    const PhysiologyData* physiology =
        d.ledger->try_get_physiology(d.player_entity_id);
    if (physiology && physiology->downed) {
        if (d.sink) d.sink->on_player_action_failed("downed");
        return false;
    }
    EquipmentData* equipment = d.ledger->try_get_equipment(d.player_entity_id);
    const EquipmentSlot* slot = equipment ? Equipment::get_slot(*equipment, slot_name) : nullptr;
    if (!slot || slot->item_id.is_empty()) return false;
    const String item_id = slot->item_id;
    if (!d.ledger->add_inventory_item(d.player_entity_id, item_id, 1)) return false;
    if (!d.ledger->unwield_weapon(d.player_entity_id, slot_name)) {
        d.ledger->remove_inventory_item(d.player_entity_id, item_id, 1);
        return false;
    }
    if (finish_short_player_action()) return true;
    d.ledger->wield_weapon(d.player_entity_id, slot_name, item_id);
    d.ledger->remove_inventory_item(d.player_entity_id, item_id, 1);
    return false;
}

bool SimulationDirector::submit_player_wear(const String& item_id) {
    if (!d.ledger || has_player_activity() ||
        d.ledger->get_inventory_item_amount(d.player_entity_id, item_id) <= 0) return false;
    if (!d.ledger->equip_clothing_by_string(d.player_entity_id, item_id)) return false;
    if (!d.ledger->remove_inventory_item(d.player_entity_id, item_id, 1)) {
        d.ledger->unequip_clothing_by_string(d.player_entity_id, item_id);
        return false;
    }
    if (finish_short_player_action()) return true;
    d.ledger->add_inventory_item(d.player_entity_id, item_id, 1);
    d.ledger->unequip_clothing_by_string(d.player_entity_id, item_id);
    return false;
}

bool SimulationDirector::submit_player_remove_clothing(const String& item_id) {
    if (!d.ledger || has_player_activity()) return false;
    const ClothingData* clothing = d.ledger->try_get_clothing(d.player_entity_id);
    if (!clothing || !Clothing::is_equipped(*clothing, item_id)) return false;
    if (!d.ledger->add_inventory_item(d.player_entity_id, item_id, 1)) return false;
    if (!d.ledger->unequip_clothing_by_string(d.player_entity_id, item_id)) {
        d.ledger->remove_inventory_item(d.player_entity_id, item_id, 1);
        return false;
    }
    if (finish_short_player_action()) return true;
    d.ledger->equip_clothing_by_string(d.player_entity_id, item_id);
    d.ledger->remove_inventory_item(d.player_entity_id, item_id, 1);
    return false;
}

void SimulationDirector::process_game_turn(float current_time) {
    if (d.ledger == nullptr || d.tracker == nullptr || d.bubble == nullptr || d.scheduler == nullptr || d.sink == nullptr) {
        return;
    }

    TileDb* tile_db = TileDb::get_singleton();
    if (!tile_db) return;

    EntityPool& pool = d.ledger->get_entity_pool();

    while (d.scheduler->peek_time() <= current_time) {
        const float event_time = d.scheduler->peek_time();
        uint32_t entity_id = d.scheduler->pop();
        if (entity_id == EntityPool::INVALID_ID) break;
        if (!settle_entity_time(entity_id, event_time)) {
            if (entity_id == d.player_entity_id) break;
            continue;
        }
        if (entity_id == d.player_entity_id) {
            d.sink->on_player_turn_ready(entity_id);
            break;
        }

        NpcTurnProcessor::run_turn(entity_id, pool, *tile_db, *this);
        const HealthData* player_health =
            d.ledger->try_get_health(d.player_entity_id);
        if (player_health && !player_health->alive) break;
    }
}

Array SimulationDirector::find_path(const Vector2i& start, const Vector2i& goal) {
    return find_path_with_flags(start, goal, 0);
}

Array SimulationDirector::request_player_path(const Vector2i& start, const Vector2i& goal) {
    if (!d.bubble->is_cell_seen(goal.x, goal.y)) {
        return Array();
    }
    return find_path_with_flags(start, goal, PATH_FLAG_ALLOW_DIAGONAL);
}

Array SimulationDirector::find_path_with_flags(const Vector2i& start, const Vector2i& goal, uint32_t flags) {
    if (!d.pathfinder) {
        return Array();
    }

    TraversalSnapshot traversal = d.bubble->build_traversal_snapshot(
        start,
        d.ledger,
        d.player_entity_id
    );
    PathRequest request;
    request.start = start;
    request.goal = goal;
    request.flags = flags;

    PathResult result = d.pathfinder->find_path(request, traversal);
    return path_result_to_array(result);
}

Vector2i SimulationDirector::entity_chunk(uint32_t entity_id) const {
    const Entity* e = d.ledger->get_entity_pool().get_entity(entity_id);
    if (e) {
        int cs = WorldCoords::CHUNK_SIZE;
        return Vector2i(floor((float)e->x / cs), floor((float)e->y / cs));
    }
    return Vector2i();
}
