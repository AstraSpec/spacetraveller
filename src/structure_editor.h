#ifndef SPACETRAVELLER_STRUCTURE_EDITOR_H
#define SPACETRAVELLER_STRUCTURE_EDITOR_H

#include "fast_tilemap.h"

namespace godot {

class StructureEditor : public FastTileMap {
    GDCLASS(StructureEditor, FastTileMap)

protected:
    static void _bind_methods();

    std::unordered_map<uint64_t, RID> preview_tile_rids;

public:
    StructureEditor();
    ~StructureEditor();

    void update_visuals(const Vector2i& centerPos);
    Dictionary export_to_rle(const String &p_id) const;
    void import_from_rle(const String &p_blueprint, const Array &p_palette);

    void update_preview_tiles(const Array &p_positions, const String &p_tile_id);
    void update_preview_tiles_with_data(const Dictionary &p_data);
    void clear_preview_tiles();
};

}

#endif // SPACETRAVELLER_STRUCTURE_EDITOR_H

