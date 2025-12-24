#ifndef SPACETRAVELLER_DATABASE_H
#define SPACETRAVELLER_DATABASE_H

#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <unordered_map>

namespace godot {

struct StringHasher {
    size_t operator()(const String &p_string) const {
        return p_string.hash();
    }
};

class DataBaseHelper {
protected:
    Dictionary full_data;
    std::unordered_map<String, Dictionary, StringHasher> fast_lookup;

    void load_json_file(const String &p_path);

public:
    DataBaseHelper();
    virtual ~DataBaseHelper();

    void load_directory(const String &p_path);
    Dictionary get_full_data() const { return full_data; }
    const std::unordered_map<String, Dictionary, StringHasher>& get_all_rows() const { return fast_lookup; }
    
    Vector2i variant_to_vector2i(const Variant &p_var, const Vector2i &p_default = Vector2i(-1, -1)) const;
    Variant get_data_value(const String &p_id, const String &p_key, const Variant &p_default = Variant()) const;
};

}

#endif // ! SPACETRAVELLER_DATABASE_H
