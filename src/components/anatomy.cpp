#include "anatomy.h"
#include "combat_math.h"
#include "data/race_db.h"
#include "data/body_part_db.h"
#include "core/tag_registry.h"
#include "core/string_hasher.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace godot {

namespace {

String canonical_race_id(const String& race_id) {
    return race_id == "mouse" ? String("rat") : race_id;
}

float clamped_integrity(const BodyPart& part) {
    return CLAMP(part.integrity, 0.0f, 1.0f);
}

bool part_has_tag(const BodyPart& part, uint16_t tag_id, BodyPartDb* db) {
    const BodyPartInfo* info = db ? db->get_body_part_info(part.type_id) : nullptr;
    return info && tag_id != 0 && TagRegistry::has_tag(tag_id, info->tags);
}

float chain_efficiency(const AnatomyData& data, int index, uint16_t core_tag, BodyPartDb* db) {
    float result = 1.0f;
    int current = index;
    while (current >= 0 && current < static_cast<int>(data.parts.size())) {
        const BodyPart& part = data.parts[current];
        if (part_has_tag(part, core_tag, db)) break;
        result *= clamped_integrity(part);
        current = part.parent_index;
    }
    return result;
}

float natural_attack_chain_integrity(
    const AnatomyData& data,
    int index,
    uint16_t core_tag,
    BodyPartDb* db
) {
    float result = 1.0f;
    int current = index;
    while (current >= 0 &&
           current < static_cast<int>(data.parts.size())) {
        const BodyPart& part = data.parts[current];
        if (part_has_tag(part, core_tag, db)) break;
        result = MIN(result, clamped_integrity(part));
        current = part.parent_index;
    }
    return result;
}

bool is_descendant_of(const AnatomyData& data, int child, int ancestor) {
    int current = child;
    while (current >= 0 && current < static_cast<int>(data.parts.size())) {
        if (current == ancestor) return true;
        current = data.parts[current].parent_index;
    }
    return false;
}

float descendant_digit_integrity(
        const AnatomyData& data, int endpoint, uint16_t digit_tag, BodyPartDb* db) {
    float sum = 0.0f;
    int count = 0;
    for (int i = 0; i < static_cast<int>(data.parts.size()); ++i) {
        if (i == endpoint || !is_descendant_of(data, i, endpoint)) continue;
        if (!part_has_tag(data.parts[i], digit_tag, db)) continue;
        sum += clamped_integrity(data.parts[i]);
        ++count;
    }
    return count > 0 ? sum / static_cast<float>(count) : 1.0f;
}

float part_pain_significance(
    const BodyPart& part,
    BodyPartDb* db,
    TagRegistry* tags
) {
    if (!db || !tags) return 0.10f;
    const BodyPartInfo* info = db->get_body_part_info(part.type_id);
    if (!info) return 0.10f;
    auto has = [&](const char* tag_name) {
        const uint16_t tag = tags->get_tag_id(tag_name);
        return tag != 0 && TagRegistry::has_tag(tag, info->tags);
    };
    if (has("CORE") || has("VITAL")) return 1.0f;
    if (has("LIMB")) return 0.65f;
    if (has("GRASP") || has("STANCE")) return 0.35f;
    if (has("SENSE") || has("SPEECH") || has("EAT")) return 0.20f;
    return 0.10f;
}

float movement_proximal_integrity(
    const AnatomyData& data,
    int endpoint,
    uint16_t core_tag,
    BodyPartDb* db
) {
    int current = endpoint >= 0 &&
            endpoint < static_cast<int>(data.parts.size())
        ? data.parts[endpoint].parent_index
        : -1;
    while (current >= 0 &&
           current < static_cast<int>(data.parts.size())) {
        const BodyPart& part = data.parts[current];
        if (part_has_tag(part, core_tag, db)) break;
        return clamped_integrity(part);
    }
    return 1.0f;
}

}

void Anatomy::init(AnatomyData& data, const String& race_id) {
    data.race_id = canonical_race_id(race_id);
    data.parts.clear();

    RaceDb* db = RaceDb::get_singleton();
    if (!db) return;

    const RaceInfo* race = db->get_race_info(data.race_id);
    if (!race) return;

    struct Pending {
        String type_id;
        String parent_type_id;
        int count;
        String height;
    };
    std::vector<Pending> pending;
    for (const auto& def : race->parts) {
        pending.push_back({def.part_id, def.parent_part_id, def.count, def.height});
    }

    auto build_recursive = [&](auto self, String parent_type, int parent_instance_idx) -> void {
        for (const auto& p : pending) {
            if (p.parent_type_id == parent_type) {
                for (int i = 0; i < p.count; i++) {
                    BodyPart part;
                    part.type_id = p.type_id;
                    part.parent_index = parent_instance_idx;
                    part.integrity = 1.0f;
                    part.local_index = i;
                    part.height = p.height;

                    int my_idx = static_cast<int>(data.parts.size());
                    data.parts.push_back(part);

                    self(self, p.type_id, my_idx);
                }
            }
        }
    };

    build_recursive(build_recursive, "", -1);
    refresh_capabilities(data);
}

int Anatomy::find_part(const AnatomyData& data, const String& type_id, int skip) {
    int skipped = 0;
    for (int i = 0; i < data.parts.size(); i++) {
        if (data.parts[i].type_id == type_id) {
            if (skipped == skip) return i;
            skipped++;
        }
    }
    return -1;
}

bool Anatomy::is_functional(const AnatomyData& data, int index) {
    if (index < 0 || index >= data.parts.size()) return false;

    const BodyPart& part = data.parts[index];
    if (part.integrity <= 0.0f) return false;

    if (part.parent_index != -1) {
        return is_functional(data, part.parent_index);
    }

    return true;
}

String Anatomy::get_type_id(const AnatomyData& data, int index) {
    if (index >= 0 && index < data.parts.size()) return data.parts[index].type_id;
    return "";
}

String Anatomy::get_name(const AnatomyData& data, int index) {
    if (index < 0 || index >= data.parts.size()) return "Invalid";

    BodyPartDb* db = BodyPartDb::get_singleton();
    String base_name = db ? db->get_body_part_name(data.parts[index].type_id) : data.parts[index].type_id;

    return base_name + " " + String::num_int64(data.parts[index].local_index + 1);
}

int Anatomy::get_parent(const AnatomyData& data, int index) {
    if (index >= 0 && index < data.parts.size()) return data.parts[index].parent_index;
    return -1;
}

float Anatomy::get_integrity(const AnatomyData& data, int index) {
    if (index >= 0 && index < data.parts.size()) return data.parts[index].integrity;
    return 0.0f;
}

void Anatomy::set_integrity(AnatomyData& data, int index, float integrity) {
    if (index >= 0 && index < data.parts.size()) {
        data.parts[index].integrity = CLAMP(integrity, 0.0f, 1.0f);
        refresh_capabilities(data);
    }
}

float Anatomy::get_part_max_integrity(const AnatomyData& data, int index) {
    if (index < 0 || index >= static_cast<int>(data.parts.size())) return 0.0f;

    BodyPartDb* body_db = BodyPartDb::get_singleton();
    RaceDb* race_db = RaceDb::get_singleton();
    const BodyPart& part = data.parts[index];
    const float base = body_db
        ? body_db->get_body_part_max_integrity(part.type_id)
        : 10.0f;

    float race_scale = 1.0f;
    float part_scale = 1.0f;
    const RaceInfo* race = race_db
        ? race_db->get_race_info(canonical_race_id(data.race_id))
        : nullptr;
    if (race) {
        race_scale = MAX(0.01f, race->anatomy_scale);
        for (const RacePartDefinition& def : race->parts) {
            if (def.part_id == part.type_id) {
                part_scale = MAX(0.01f, def.integrity_scale);
                break;
            }
        }
    }
    return CombatMath::scaled_part_integrity(base, race_scale, part_scale);
}

float Anatomy::get_part_current_integrity(const AnatomyData& data, int index) {
    if (index < 0 || index >= static_cast<int>(data.parts.size())) return 0.0f;
    return get_part_max_integrity(data, index) * clamped_integrity(data.parts[index]);
}

PartDamageResult Anatomy::apply_damage(AnatomyData& data, int index, float damage) {
    PartDamageResult result;
    if (index < 0 || index >= static_cast<int>(data.parts.size()) || damage <= 0.0f) {
        if (index >= 0 && index < static_cast<int>(data.parts.size())) {
            result.remaining_integrity = clamped_integrity(data.parts[index]);
            result.lethal = has_lethal_failure(data);
        }
        return result;
    }

    const float max_integrity = get_part_max_integrity(data, index);
    if (max_integrity <= 0.0f) {
        result.remaining_integrity = clamped_integrity(data.parts[index]);
        result.lethal = has_lethal_failure(data);
        return result;
    }

    const float before = clamped_integrity(data.parts[index]);
    const float current_absolute = before * max_integrity;
    result.applied_damage = MIN(damage, current_absolute);
    result.remaining_integrity =
        CLAMP(before - result.applied_damage / max_integrity, 0.0f, 1.0f);
    result.newly_destroyed = before > 0.0f && result.remaining_integrity <= 0.0f;
    set_integrity(data, index, result.remaining_integrity);
    result.lethal = has_lethal_failure(data);
    return result;
}

float Anatomy::get_body_integrity(const AnatomyData& data) {
    BodyPartDb* db = BodyPartDb::get_singleton();
    if (!db || data.parts.empty()) return 0.0f;

    float weighted = 0.0f;
    float total_weight = 0.0f;
    for (const BodyPart& part : data.parts) {
        const float weight = MAX(0.0f, db->get_body_part_hit_weight(part.type_id));
        weighted += weight * clamped_integrity(part);
        total_weight += weight;
    }
    return total_weight > 0.0f ? CLAMP(weighted / total_weight, 0.0f, 1.0f) : 0.0f;
}

float Anatomy::get_wound_pain_floor(
    const AnatomyData& data,
    float total_bleed
) {
    BodyPartDb* db = BodyPartDb::get_singleton();
    TagRegistry* tags = TagRegistry::get_singleton();
    if (!db || !tags) {
        return CombatMath::combined_pain_floor({}, total_bleed);
    }

    std::vector<float> sources;
    sources.reserve(data.parts.size());
    for (const BodyPart& part : data.parts) {
        const float missing = 1.0f - clamped_integrity(part);
        const float multiplier =
            db->get_body_part_pain_multiplier(part.type_id);
        sources.push_back(CombatMath::wound_pain_source(
            missing,
            multiplier,
            part_pain_significance(part, db, tags)));
    }
    return CombatMath::combined_pain_floor(sources, total_bleed);
}

bool Anatomy::has_lethal_failure(const AnatomyData& data) {
    BodyPartDb* db = BodyPartDb::get_singleton();
    TagRegistry* tags = TagRegistry::get_singleton();
    if (!db || !tags) return false;

    const uint16_t core_tag = tags->get_tag_id("CORE");
    const uint16_t vital_tag = tags->get_tag_id("VITAL");
    for (const BodyPart& part : data.parts) {
        if (clamped_integrity(part) > 0.0f) continue;
        const BodyPartInfo* info = db->get_body_part_info(part.type_id);
        if (!info) continue;
        if ((core_tag != 0 && TagRegistry::has_tag(core_tag, info->tags)) ||
            (vital_tag != 0 && TagRegistry::has_tag(vital_tag, info->tags))) {
            return true;
        }
    }
    return false;
}

int Anatomy::get_count(const AnatomyData& data) {
    return static_cast<int>(data.parts.size());
}

Dictionary Anatomy::get_functional_list(const AnatomyData& data) {
    Dictionary result;
    Array list;
    for (int i = 0; i < data.parts.size(); i++) {
        if (is_functional(data, i)) {
            Dictionary d;
            d["index"] = i;
            d["type_id"] = data.parts[i].type_id;
            d["name"] = get_name(data, i);
            list.push_back(d);
        }
    }
    result["parts"] = list;
    return result;
}

bool Anatomy::has_functional_limbs(const AnatomyData& data, const std::vector<String>& required) {
    // Count how many of each required type are needed.
    std::unordered_map<String, int, StringHasher> needed;
    for (const String& r : required) needed[r]++;

    for (const auto& pair : needed) {
        int available = 0;
        for (int i = 0; i < data.parts.size(); i++) {
            if (data.parts[i].type_id == pair.first && is_functional(data, i)) {
                available++;
            }
        }
        if (available < pair.second) return false;
    }
    return true;
}

NaturalLimbAllocation Anatomy::allocate_natural_attack_limbs(
    const AnatomyData& data,
    const std::vector<String>& required
) {
    NaturalLimbAllocation result;
    if (required.empty()) {
        result.valid = true;
        result.raw_integrity = 1.0f;
        result.ratio = 1.0f;
        return result;
    }

    BodyPartDb* db = BodyPartDb::get_singleton();
    TagRegistry* tags = TagRegistry::get_singleton();
    if (!db || !tags) return result;
    const uint16_t core_tag = tags->get_tag_id("CORE");

    result.raw_integrity = 1.0f;
    result.part_indices.reserve(required.size());
    for (const String& required_type : required) {
        int best_index = -1;
        float best_integrity = -1.0f;
        for (int i = 0; i < static_cast<int>(data.parts.size()); ++i) {
            if (data.parts[i].type_id != required_type ||
                !is_functional(data, i) ||
                std::find(
                    result.part_indices.begin(),
                    result.part_indices.end(),
                    i) != result.part_indices.end()) {
                continue;
            }
            const float integrity = natural_attack_chain_integrity(
                data,
                i,
                core_tag,
                db);
            if (integrity > best_integrity) {
                best_integrity = integrity;
                best_index = i;
            }
        }
        if (best_index < 0) {
            result.part_indices.clear();
            result.raw_integrity = 0.0f;
            result.ratio = 0.0f;
            return result;
        }
        result.part_indices.push_back(best_index);
        result.raw_integrity = MIN(result.raw_integrity, best_integrity);
    }

    result.valid = true;
    result.raw_integrity = CLAMP(result.raw_integrity, 0.0f, 1.0f);
    result.ratio =
        CombatMath::natural_limb_handling_ratio(result.raw_integrity);
    return result;
}

int Anatomy::count_functional_parts_with_tag(const AnatomyData& data, const String& tag) {
    BodyPartDb* db = BodyPartDb::get_singleton();
    TagRegistry* tags = TagRegistry::get_singleton();
    if (!db || !tags) return 0;

    uint16_t tag_id = tags->get_tag_id(tag);
    if (tag_id == 0) return 0;

    int count = 0;
    for (int i = 0; i < data.parts.size(); i++) {
        if (!Anatomy::is_functional(data, i)) continue;
        const BodyPartInfo* info = db->get_body_part_info(data.parts[i].type_id);
        if (info && TagRegistry::has_tag(tag_id, info->tags)) count++;
    }
    return count;
}

float Anatomy::min_required_integrity(const AnatomyData& data, const std::vector<String>& required) {
    const NaturalLimbAllocation allocation =
        allocate_natural_attack_limbs(data, required);
    return allocation.valid ? allocation.raw_integrity : 0.0f;
}

void Anatomy::refresh_capabilities(AnatomyData& data) {
    data.capabilities = BodyCapabilities();

    BodyPartDb* db = BodyPartDb::get_singleton();
    TagRegistry* tags = TagRegistry::get_singleton();
    if (!db || !tags || data.parts.empty()) return;

    const uint16_t core_tag = tags->get_tag_id("CORE");
    const uint16_t stance_tag = tags->get_tag_id("STANCE");
    const uint16_t grasp_tag = tags->get_tag_id("GRASP");
    const uint16_t sight_tag = tags->get_tag_id("SIGHT");
    const uint16_t speech_tag = tags->get_tag_id("SPEECH");
    const uint16_t digit_tag = tags->get_tag_id("DIGIT");

    float moving_sum = 0.0f;
    int moving_count = 0;
    float manipulation_sum = 0.0f;
    int manipulation_count = 0;
    std::vector<float> perception_sources;
    float speech = 1.0f;
    int speech_count = 0;

    for (int i = 0; i < static_cast<int>(data.parts.size()); ++i) {
        const BodyPart& part = data.parts[i];
        const float chain = chain_efficiency(data, i, core_tag, db);

        if (part_has_tag(part, stance_tag, db)) {
            const float endpoint = clamped_integrity(part);
            const float digits = endpoint > 0.0f
                ? descendant_digit_integrity(data, i, digit_tag, db)
                : 0.0f;
            const float proximal = movement_proximal_integrity(
                data,
                i,
                core_tag,
                db);
            moving_sum += CombatMath::movement_endpoint_capacity(
                proximal,
                endpoint,
                digits);
            ++moving_count;
        }
        if (part_has_tag(part, grasp_tag, db)) {
            const float digits = descendant_digit_integrity(data, i, digit_tag, db);
            manipulation_sum += chain * (0.2f + 0.8f * digits);
            ++manipulation_count;
        }
        if (part_has_tag(part, sight_tag, db)) {
            perception_sources.push_back(chain);
        }
        if (part_has_tag(part, speech_tag, db)) {
            speech = MIN(speech, chain);
            ++speech_count;
        }
    }

    if (moving_count > 0) {
        data.capabilities.moving = moving_sum / static_cast<float>(moving_count);
    }
    if (manipulation_count > 0) {
        data.capabilities.manipulation = manipulation_sum / static_cast<float>(manipulation_count);
    }
    if (!perception_sources.empty()) {
        std::sort(perception_sources.begin(), perception_sources.end(), std::greater<float>());
        if (perception_sources.size() == 1) {
            data.capabilities.perception = perception_sources[0];
        } else {
            float remaining = 0.0f;
            for (size_t i = 1; i < perception_sources.size(); ++i) {
                remaining += perception_sources[i];
            }
            remaining /= static_cast<float>(perception_sources.size() - 1);
            data.capabilities.perception = 0.75f * perception_sources[0] + 0.25f * remaining;
        }
    }
    if (speech_count > 0) {
        data.capabilities.speech = speech;
    }
}

float Anatomy::get_moving_capacity(const AnatomyData& data) {
    return data.capabilities.moving;
}

float Anatomy::get_manipulation_capacity(const AnatomyData& data) {
    return data.capabilities.manipulation;
}

float Anatomy::get_manipulation_units(const AnatomyData& data) {
    BodyPartDb* db = BodyPartDb::get_singleton();
    TagRegistry* tags = TagRegistry::get_singleton();
    if (!db || !tags) return 0.0f;

    const uint16_t core_tag = tags->get_tag_id("CORE");
    const uint16_t grasp_tag = tags->get_tag_id("GRASP");
    const uint16_t digit_tag = tags->get_tag_id("DIGIT");
    float units = 0.0f;
    for (int i = 0; i < static_cast<int>(data.parts.size()); ++i) {
        if (!part_has_tag(data.parts[i], grasp_tag, db)) continue;
        const float chain = chain_efficiency(data, i, core_tag, db);
        const float digits = descendant_digit_integrity(data, i, digit_tag, db);
        units += CombatMath::manipulation_endpoint_units(chain, digits);
    }
    return MAX(0.0f, units);
}

float Anatomy::get_perception_capacity(const AnatomyData& data) {
    return data.capabilities.perception;
}

float Anatomy::get_speech_capacity(const AnatomyData& data) {
    return data.capabilities.speech;
}

Dictionary Anatomy::get_capabilities(const AnatomyData& data) {
    Dictionary result;
    result["moving"] = data.capabilities.moving;
    result["manipulation"] = data.capabilities.manipulation;
    result["perception"] = data.capabilities.perception;
    result["speech"] = data.capabilities.speech;
    return result;
}

static int pick_hit_location_internal(
    const AnatomyData& data,
    const std::vector<String>& preferred_heights,
    float pool_roll_unit,
    float location_roll_unit
) {
    BodyPartDb* db = BodyPartDb::get_singleton();
    if (!db) return -1;

    auto height_match = [&](int i) -> bool {
        if (preferred_heights.empty()) return true;
        for (const String& h : preferred_heights) {
            if (data.parts[i].height == h) return true;
        }
        return false;
    };

    float preferred_total = 0.0f;
    float all_total = 0.0f;
    for (int i = 0; i < data.parts.size(); i++) {
        if (!Anatomy::is_functional(data, i)) continue;
        const float weight =
            db->get_body_part_hit_weight(data.parts[i].type_id);
        all_total += weight;
        if (height_match(i)) preferred_total += weight;
    }

    const bool use_height_filter =
        !preferred_heights.empty() &&
        CombatMath::use_preferred_height_pool(
            preferred_total > 0.0f,
            pool_roll_unit);
    const float total =
        use_height_filter ? preferred_total : all_total;
    if (total <= 0.0f) return -1;

    float roll = location_roll_unit * total;
    for (int i = 0; i < data.parts.size(); i++) {
        if (!Anatomy::is_functional(data, i)) continue;
        if (use_height_filter && !height_match(i)) continue;
        roll -= db->get_body_part_hit_weight(data.parts[i].type_id);
        if (roll <= 0.0f) return i;
    }
    return -1;
}

int Anatomy::pick_hit_location(const AnatomyData& data, const std::vector<String>& preferred_heights) {
    return pick_hit_location_internal(
        data,
        preferred_heights,
        static_cast<float>(UtilityFunctions::randf()),
        static_cast<float>(UtilityFunctions::randf()));
}

int Anatomy::pick_hit_location(const AnatomyData& data, const std::vector<String>& preferred_heights, Rng::Seeded& rng) {
    return pick_hit_location_internal(
        data,
        preferred_heights,
        rng.unit(),
        rng.unit());
}

float Anatomy::get_targeting_penalty(
    const AnatomyData& data,
    int target_index
) {
    BodyPartDb* db = BodyPartDb::get_singleton();
    if (!db || !is_functional(data, target_index)) return 0.35f;
    float total = 0.0f;
    for (int i = 0; i < static_cast<int>(data.parts.size()); ++i) {
        if (!is_functional(data, i)) continue;
        total += MAX(
            0.0f,
            db->get_body_part_hit_weight(data.parts[i].type_id));
    }
    const float target_weight = MAX(
        0.0f,
        db->get_body_part_hit_weight(data.parts[target_index].type_id));
    return CombatMath::targeted_location_penalty(target_weight, total);
}

int Anatomy::pick_deviation_location(
    const AnatomyData& data,
    int target_index,
    Rng::Seeded& rng
) {
    BodyPartDb* db = BodyPartDb::get_singleton();
    if (!db || target_index < 0 ||
        target_index >= static_cast<int>(data.parts.size())) {
        return -1;
    }

    auto weighted_pick = [&](const std::vector<int>& candidates) -> int {
        float total = 0.0f;
        for (int index : candidates) {
            total += MAX(
                0.0f,
                db->get_body_part_hit_weight(data.parts[index].type_id));
        }
        if (total <= 0.0f) return -1;
        float roll = rng.unit() * total;
        for (int index : candidates) {
            roll -= MAX(
                0.0f,
                db->get_body_part_hit_weight(data.parts[index].type_id));
            if (roll <= 0.0f) return index;
        }
        return candidates.empty() ? -1 : candidates.back();
    };

    auto append_candidate = [&](
        std::vector<int>& candidates,
        std::unordered_set<int>& seen,
        int index
    ) {
        if (index < 0 || index == target_index ||
            index >= static_cast<int>(data.parts.size()) ||
            !is_functional(data, index) || seen.count(index) != 0) {
            return;
        }
        seen.insert(index);
        candidates.push_back(index);
    };

    std::vector<int> candidates;
    std::unordered_set<int> seen;
    const int parent = data.parts[target_index].parent_index;
    append_candidate(candidates, seen, parent);
    for (int i = 0; i < static_cast<int>(data.parts.size()); ++i) {
        if (data.parts[i].parent_index == target_index ||
            (parent >= 0 && data.parts[i].parent_index == parent)) {
            append_candidate(candidates, seen, i);
        }
    }
    int selected = weighted_pick(candidates);
    if (selected >= 0) return selected;

    candidates.clear();
    seen.clear();
    const String height = data.parts[target_index].height;
    for (int i = 0; i < static_cast<int>(data.parts.size()); ++i) {
        if (data.parts[i].height == height) {
            append_candidate(candidates, seen, i);
        }
    }
    selected = weighted_pick(candidates);
    if (selected >= 0) return selected;

    candidates.clear();
    seen.clear();
    for (int i = 0; i < static_cast<int>(data.parts.size()); ++i) {
        append_candidate(candidates, seen, i);
    }
    return weighted_pick(candidates);
}

Dictionary Anatomy::serialize(const AnatomyData& data) {
    Dictionary result;
    result["race_id"] = data.race_id;
    result["capacities"] = get_capabilities(data);
    Array parts;
    for (const auto& part : data.parts) {
        Dictionary d;
        d["type_id"] = part.type_id;
        d["parent_index"] = part.parent_index;
        d["integrity"] = part.integrity;
        d["local_index"] = part.local_index;
        d["height"] = part.height;
        parts.push_back(d);
    }
    result["parts"] = parts;
    return result;
}

void Anatomy::deserialize(AnatomyData& data, const Dictionary& dict) {
    data.race_id = canonical_race_id(dict.get("race_id", ""));
    data.parts.clear();
    Array parts = dict.get("parts", Array());
    for (int i = 0; i < parts.size(); i++) {
        Dictionary d = parts[i];
        BodyPart part;
        part.type_id = d.get("type_id", "");
        part.parent_index = d.get("parent_index", -1);
        part.integrity = d.get("integrity", 1.0f);
        part.local_index = d.get("local_index", 0);
        part.height = d.get("height", "MID");
        data.parts.push_back(part);
    }
    refresh_capabilities(data);
}

}
