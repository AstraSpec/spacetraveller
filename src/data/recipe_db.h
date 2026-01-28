#ifndef SPACETRAVELLER_RECIPE_DB_H
#define SPACETRAVELLER_RECIPE_DB_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>
#include <vector>
#include "database.h"

namespace godot {

struct RecipeRequirement {
    String item_id;
    int amount;
};

struct RecipeResult {
    String item_id;
    int amount;
};

struct RecipeInfo {
    String name;
    String description;
    std::vector<RecipeRequirement> requirements;
    std::vector<RecipeResult> results;
    float time_seconds = 0.0f;
};

class RecipeDb : public Object, public DataBase<RecipeInfo, RecipeDb> {
    GDCLASS(RecipeDb, Object)

protected:
    static void _bind_methods();
    virtual RecipeInfo _parse_row(const Dictionary &p_data) override;

public:
    RecipeDb();
    ~RecipeDb();

    void initialize_data() { DataBase<RecipeInfo, RecipeDb>::initialize_data("res://data/recipes"); }
    Array get_ids() const { return DataBase<RecipeInfo, RecipeDb>::get_ids(); }

    String get_recipe_name(const String &p_id) const;
    String get_recipe_description(const String &p_id) const;
    Array get_recipe_requirements(const String &p_id) const;
    Array get_recipe_results(const String &p_id) const;
    float get_recipe_time(const String &p_id) const;
};

}

#endif // ! SPACETRAVELLER_RECIPE_DB_H
