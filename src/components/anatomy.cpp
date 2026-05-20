#include "anatomy.h"
#include "data/race_db.h"
#include "data/body_part_db.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

void Anatomy::_bind_methods() {
    ClassDB::bind_method(D_METHOD("initialize_from_race", "race_id"), &Anatomy::initialize_from_race);
    ClassDB::bind_method(D_METHOD("find_part_of_type", "type_id", "skip_count"), &Anatomy::find_part_of_type, DEFVAL(0));
    ClassDB::bind_method(D_METHOD("is_part_functional", "index"), &Anatomy::is_part_functional);
    ClassDB::bind_method(D_METHOD("get_part_count"), &Anatomy::get_part_count);
    ClassDB::bind_method(D_METHOD("get_part_type_id", "index"), &Anatomy::get_part_type_id);
    ClassDB::bind_method(D_METHOD("get_part_name", "index"), &Anatomy::get_part_name);
    ClassDB::bind_method(D_METHOD("get_part_parent", "index"), &Anatomy::get_part_parent);
    ClassDB::bind_method(D_METHOD("get_part_integrity", "index"), &Anatomy::get_part_integrity);
    ClassDB::bind_method(D_METHOD("set_part_integrity", "index", "integrity"), &Anatomy::set_part_integrity);
    ClassDB::bind_method(D_METHOD("get_functional_parts_list"), &Anatomy::get_functional_parts_list);

    ClassDB::bind_method(D_METHOD("get_save_data"), &Anatomy::get_save_data);
    ClassDB::bind_method(D_METHOD("load_save_data", "data"), &Anatomy::load_save_data);
}

Anatomy::Anatomy() {}
Anatomy::~Anatomy() {}

void Anatomy::initialize_from_race(const String &p_race_id) {
    race_id = p_race_id;
    instances.clear();

    RaceDb *db = RaceDb::get_singleton();
    if (!db) return;

    const RaceInfo *race = db->get_race_info(p_race_id);
    if (!race) return;
    
    // 1. Collect all definitions
    struct Pending {
        String type_id;
        String parent_type_id;
        int count;
    };
    std::vector<Pending> pending;
    for (const auto &def : race->parts) {
        pending.push_back({def.part_id, def.parent_part_id, def.count});
    }

    // 2. Recursive build helper
    auto build_recursive = [&](auto self, String parent_type, int parent_instance_idx) -> void {
        for (const auto &p : pending) {
            if (p.parent_type_id == parent_type) {
                for (int i = 0; i < p.count; i++) {
                    PartInstance inst;
                    inst.type_id = p.type_id;
                    inst.parent_index = parent_instance_idx;
                    inst.integrity = 1.0f;
                    inst.local_index = i;
                    
                    int my_idx = static_cast<int>(instances.size());
                    instances.push_back(inst);
                    
                    self(self, p.type_id, my_idx);
                }
            }
        }
    };

    build_recursive(build_recursive, "", -1);
}

int Anatomy::find_part_of_type(const String &p_type_id, int p_skip_count) const {
    int skipped = 0;
    for (int i = 0; i < instances.size(); i++) {
        if (instances[i].type_id == p_type_id) {
            if (skipped == p_skip_count) {
                return i;
            }
            skipped++;
        }
    }
    return -1;
}

bool Anatomy::is_part_functional(int p_index) const {
    if (p_index < 0 || p_index >= instances.size()) return false;
    
    const PartInstance &inst = instances[p_index];
    if (inst.integrity <= 0.0f) return false;
    
    if (inst.parent_index != -1) {
        return is_part_functional(inst.parent_index);
    }
    
    return true;
}

String Anatomy::get_part_type_id(int p_index) const {
    if (p_index >= 0 && p_index < instances.size()) return instances[p_index].type_id;
    return "";
}

String Anatomy::get_part_name(int p_index) const {
    if (p_index < 0 || p_index >= instances.size()) return "Invalid";
    
    BodyPartDb *db = BodyPartDb::get_singleton();
    String base_name = db ? db->get_body_part_name(instances[p_index].type_id) : instances[p_index].type_id;
    
    return base_name + " " + String::num_int64(instances[p_index].local_index + 1);
}

int Anatomy::get_part_parent(int p_index) const {
    if (p_index >= 0 && p_index < instances.size()) return instances[p_index].parent_index;
    return -1;
}

float Anatomy::get_part_integrity(int p_index) const {
    if (p_index >= 0 && p_index < instances.size()) return instances[p_index].integrity;
    return 0.0f;
}

void Anatomy::set_part_integrity(int p_index, float p_integrity) {
    if (p_index >= 0 && p_index < instances.size()) {
        instances[p_index].integrity = p_integrity;
    }
}

Array Anatomy::get_functional_parts_list() const {
    Array list;
    for (int i = 0; i < instances.size(); i++) {
        if (is_part_functional(i)) {
            Dictionary d;
            d["index"] = i;
            d["type_id"] = instances[i].type_id;
            d["name"] = get_part_name(i);
            list.push_back(d);
        }
    }
    return list;
}

Dictionary Anatomy::get_save_data() const {
    Dictionary data;
    data["race_id"] = race_id;
    Array insts;
    for (const auto& inst : instances) {
        Dictionary d;
        d["type_id"] = inst.type_id;
        d["parent_index"] = inst.parent_index;
        d["integrity"] = inst.integrity;
        d["local_index"] = inst.local_index;
        insts.push_back(d);
    }
    data["instances"] = insts;
    return data;
}

void Anatomy::load_save_data(const Dictionary &p_data) {
    race_id = p_data.get("race_id", "");
    instances.clear();
    Array insts = p_data.get("instances", Array());
    for (int i = 0; i < insts.size(); i++) {
        Dictionary d = insts[i];
        PartInstance inst;
        inst.type_id = d.get("type_id", "");
        inst.parent_index = d.get("parent_index", -1);
        inst.integrity = d.get("integrity", 1.0f);
        inst.local_index = d.get("local_index", 0);
        instances.push_back(inst);
    }
}

}
