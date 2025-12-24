#include "database.h"
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

DataBaseHelper::DataBaseHelper() {}
DataBaseHelper::~DataBaseHelper() {}

void DataBaseHelper::load_directory(const String &p_path) {
    Ref<DirAccess> dir = DirAccess::open(p_path);
    if (dir.is_null()) {
        UtilityFunctions::printerr("Failed to open directory: ", p_path);
        return;
    }

    dir->list_dir_begin();
    String file_name = dir->get_next();
    while (file_name != "") {
        if (dir->current_is_dir()) {
            if (file_name != "." && file_name != "..") {
                load_directory(p_path.path_join(file_name));
            }
        } else {
            if (file_name.ends_with(".json")) {
                load_json_file(p_path.path_join(file_name));
            }
        }
        file_name = dir->get_next();
    }
}

void DataBaseHelper::load_json_file(const String &p_path) {
    String json_text = FileAccess::get_file_as_string(p_path);
    if (json_text.is_empty()) {
        UtilityFunctions::printerr("Failed to read JSON file or file is empty: ", p_path);
        return;
    }

    Ref<JSON> json;
    json.instantiate();
    
    Error err = json->parse(json_text);
    if (err != OK) {
        UtilityFunctions::printerr("Failed to parse JSON file: ", p_path, " Error: ", json->get_error_message(), " at line: ", json->get_error_line());
        return;
    }

    Variant data = json->get_data();
    if (data.get_type() == Variant::ARRAY) {
        Array arr = data;
        for (int i = 0; i < arr.size(); i++) {
            Dictionary item = arr[i];
            if (item.has("id")) {
                String id = item["id"];
                full_data[id] = item;
                fast_lookup[id] = item;
            }
        }
    } else if (data.get_type() == Variant::DICTIONARY) {
        Dictionary dict = data;
        if (dict.has("id")) {
            String id = dict["id"];
            full_data[id] = dict;
            fast_lookup[id] = dict;
        } else {
            full_data.merge(dict);
            // For merged dicts without IDs, we can't easily add to fast_lookup
            // But usually, we care about the ID-based rows.
        }
    }
}

Vector2i DataBaseHelper::variant_to_vector2i(const Variant &p_var, const Vector2i &p_default) const {
    if (p_var.get_type() == Variant::ARRAY) {
        Array arr = p_var;
        if (arr.size() >= 2) {
            return Vector2i(arr[0], arr[1]);
        }
    }
    return p_default;
}

Variant DataBaseHelper::get_data_value(const String &p_id, const String &p_key, const Variant &p_default) const {
    auto it = fast_lookup.find(p_id);
    if (it != fast_lookup.end()) {
        const Dictionary &d = it->second;
        return d.get(p_key, p_default);
    }
    return p_default;
}

}
