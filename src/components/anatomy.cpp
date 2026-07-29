#include "anatomy.h"
#include "data/race_db.h"
#include "data/body_part_db.h"
#include "core/tag_registry.h"
#include "core/string_hasher.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>
#include <unordered_map>

namespace godot {

namespace {

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

}

void Anatomy::init(AnatomyData& data, const String& race_id) {
    data.race_id = race_id;
    data.parts.clear();

    RaceDb* db = RaceDb::get_singleton();
    if (!db) return;

    const RaceInfo* race = db->get_race_info(race_id);
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
    float worst = 1.0f;
    bool any = false;
    for (const String& r : required) {
        // Use the best functional instance of this type, but track the worst across required types.
        float best_for_type = 0.0f;
        for (int i = 0; i < data.parts.size(); i++) {
            if (data.parts[i].type_id == r && is_functional(data, i)) {
                best_for_type = MAX(best_for_type, get_integrity(data, i));
            }
        }
        if (best_for_type > 0.0f) {
            worst = MIN(worst, best_for_type);
            any = true;
        }
    }
    return any ? worst : 1.0f;
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
            const float digits = descendant_digit_integrity(data, i, digit_tag, db);
            moving_sum += chain * (0.6f + 0.4f * digits);
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

static int pick_hit_location_internal(const AnatomyData& data, const std::vector<String>& preferred_heights, float roll_unit) {
    BodyPartDb* db = BodyPartDb::get_singleton();
    if (!db) return -1;

    auto height_match = [&](int i) -> bool {
        if (preferred_heights.empty()) return true;
        for (const String& h : preferred_heights) {
            if (data.parts[i].height == h) return true;
        }
        return false;
    };

    // First pass: only parts matching preferred heights. If none match, fall back to all.
    float total = 0.0f;
    for (int i = 0; i < data.parts.size(); i++) {
        if (!Anatomy::is_functional(data, i)) continue;
        if (!height_match(i)) continue;
        total += db->get_body_part_hit_weight(data.parts[i].type_id);
    }

    bool use_height_filter = total > 0.0f;
    if (!use_height_filter) {
        for (int i = 0; i < data.parts.size(); i++) {
            if (!Anatomy::is_functional(data, i)) continue;
            total += db->get_body_part_hit_weight(data.parts[i].type_id);
        }
    }
    if (total <= 0.0f) return -1;

    float roll = roll_unit * total;
    for (int i = 0; i < data.parts.size(); i++) {
        if (!Anatomy::is_functional(data, i)) continue;
        if (use_height_filter && !height_match(i)) continue;
        roll -= db->get_body_part_hit_weight(data.parts[i].type_id);
        if (roll <= 0.0f) return i;
    }
    return -1;
}

int Anatomy::pick_hit_location(const AnatomyData& data, const std::vector<String>& preferred_heights) {
    return pick_hit_location_internal(data, preferred_heights, static_cast<float>(UtilityFunctions::randf()));
}

int Anatomy::pick_hit_location(const AnatomyData& data, const std::vector<String>& preferred_heights, Rng::Seeded& rng) {
    return pick_hit_location_internal(data, preferred_heights, rng.unit());
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
    data.race_id = dict.get("race_id", "");
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
