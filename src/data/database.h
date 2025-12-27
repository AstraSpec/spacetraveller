#ifndef SPACETRAVELLER_DATABASE_H
#define SPACETRAVELLER_DATABASE_H

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <unordered_map>

namespace godot {

struct StringHasher {
    size_t operator()(const String &p_string) const {
        return p_string.hash();
    }
};

template <typename T, typename Derived>
class DataBase {
protected:
    static Derived *singleton;
    std::unordered_map<String, T, StringHasher> cache;

    virtual T _parse_row(const Dictionary &p_data) = 0;

    void load_json_file(const String &p_path) {
        String json_text = FileAccess::get_file_as_string(p_path);
        if (json_text.is_empty()) return;

        Ref<JSON> json;
        json.instantiate();
        if (json->parse(json_text) != OK) return;

        Variant data = json->get_data();
        if (data.get_type() == Variant::ARRAY) {
            Array arr = data;
            for (int i = 0; i < arr.size(); i++) {
                Dictionary item = arr[i];
                if (item.has("id")) {
                    cache[item["id"]] = _parse_row(item);
                }
            }
        } else if (data.get_type() == Variant::DICTIONARY) {
            Dictionary dict = data;
            if (dict.has("id")) {
                cache[dict["id"]] = _parse_row(dict);
            } else {
                Array keys = dict.keys();
                for (int i = 0; i < keys.size(); i++) {
                    Variant val = dict[keys[i]];
                    if (val.get_type() == Variant::DICTIONARY) {
                        Dictionary item = val;
                        if (!item.has("id")) item["id"] = keys[i];
                        cache[keys[i]] = _parse_row(item);
                    }
                }
            }
        }
    }

public:
    static Derived *get_singleton() { return singleton; };
    
    static void create_singleton() {
        if (!singleton) singleton = memnew(Derived);
    }
    
    static void delete_singleton() {
        if (singleton) {
            memdelete(singleton);
            singleton = nullptr;
        }
    }

    void load_directory(const String &p_path) {
        Ref<DirAccess> dir = DirAccess::open(p_path);
        if (dir.is_null()) return;

        dir->list_dir_begin();
        String file_name = dir->get_next();
        while (file_name != "") {
            if (dir->current_is_dir()) {
                if (file_name != "." && file_name != "..") {
                    load_directory(p_path.path_join(file_name));
                }
            } else if (file_name.ends_with(".json")) {
                load_json_file(p_path.path_join(file_name));
            }
            file_name = dir->get_next();
        }
    }

    void initialize_data(const String &p_path) {
        cache.clear();
        load_directory(p_path);
        UtilityFunctions::print(Derived::get_class_static(), " initialized with ", cache.size(), " items from ", p_path);
    }

    const T* get_info(const String &p_id) const {
        auto it = cache.find(p_id);
        return (it != cache.end()) ? &it->second : nullptr;
    }

    Array get_ids() const {
        Array ids;
        for (const auto& pair : cache) {
            ids.push_back(pair.first);
        }
        return ids;
    }

    Vector2i variant_to_vector2i(const Variant &p_var, const Vector2i &p_default = Vector2i(-1, -1)) const {
        if (p_var.get_type() == Variant::ARRAY) {
            Array arr = p_var;
            if (arr.size() >= 2) return Vector2i(arr[0], arr[1]);
        } else if (p_var.get_type() == Variant::VECTOR2I) {
            return p_var;
        }
        return p_default;
    }
};

}

#endif // ! SPACETRAVELLER_DATABASE_H
