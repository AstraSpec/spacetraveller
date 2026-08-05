#ifndef SPACETRAVELLER_TOOL_QUALITY_DB_H
#define SPACETRAVELLER_TOOL_QUALITY_DB_H

#include <godot_cpp/classes/object.hpp>
#include "database.h"

namespace godot {

struct ToolQualityInfo {
    String name;
};

class ToolQualityDb : public Object,
                      public DataBase<ToolQualityInfo, ToolQualityDb> {
    GDCLASS(ToolQualityDb, Object)

protected:
    static void _bind_methods();
    ToolQualityInfo _parse_row(const Dictionary& p_data) override;

public:
    ToolQualityDb() = default;
    ~ToolQualityDb() = default;

    void initialize_data() {
        DataBase<ToolQualityInfo, ToolQualityDb>::initialize_data(
            "res://data/tool_qualities");
    }
    Array get_ids() const { return DataBase::get_ids(); }
    const ToolQualityInfo* get_quality_info(const String& p_id) const;
    String get_display_name(const String& p_id) const;
};

}

#endif
