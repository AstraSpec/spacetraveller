#include "anatomy.h"
#include "data/race_db.h"
#include "data/body_part_db.h"

namespace godot {

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
    };
    std::vector<Pending> pending;
    for (const auto& def : race->parts) {
        pending.push_back({def.part_id, def.parent_part_id, def.count});
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

                    int my_idx = static_cast<int>(data.parts.size());
                    data.parts.push_back(part);

                    self(self, p.type_id, my_idx);
                }
            }
        }
    };

    build_recursive(build_recursive, "", -1);
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
        data.parts[index].integrity = integrity;
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

Dictionary Anatomy::serialize(const AnatomyData& data) {
    Dictionary result;
    result["race_id"] = data.race_id;
    Array parts;
    for (const auto& part : data.parts) {
        Dictionary d;
        d["type_id"] = part.type_id;
        d["parent_index"] = part.parent_index;
        d["integrity"] = part.integrity;
        d["local_index"] = part.local_index;
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
        data.parts.push_back(part);
    }
}

}