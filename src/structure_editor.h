#ifndef SPACETRAVELLER_STRUCTURE_EDITOR_H
#define SPACETRAVELLER_STRUCTURE_EDITOR_H

#include "fast_tilemap.h"

namespace godot {

class StructureEditor : public FastTileMap {
    GDCLASS(StructureEditor, FastTileMap)

public:
    enum ShapeType {
        SHAPE_RECTANGLE,
        SHAPE_ELLIPSIS
    };

protected:
    static void _bind_methods();

    std::unordered_map<uint64_t, RID> preview_tile_rids;
    
    std::vector<Vector2i> _get_shape_points(ShapeType p_type, const Vector2i &p_p1, const Vector2i &p_p2, bool p_filled, bool p_perfect);

public:
    StructureEditor();
    ~StructureEditor();

    void update_visuals(const Vector2i& centerPos);
    Dictionary export_to_rle(const String &p_id) const;
    void import_from_rle(const String &p_blueprint, const Array &p_palette);

    void update_preview_tiles(const Array &p_positions, const String &p_tile_id);
    void update_preview_tiles_with_data(const Dictionary &p_data);
    void update_preview_shape(ShapeType p_type, const Vector2i &p_p1, const Vector2i &p_p2, bool p_filled, bool p_perfect, const String &p_tile_id);
    void commit_shape(ShapeType p_type, const Vector2i &p_p1, const Vector2i &p_p2, bool p_filled, bool p_perfect, const String &p_tile_id);
    void clear_preview_tiles();
};

}

VARIANT_ENUM_CAST(StructureEditor::ShapeType);

#endif // SPACETRAVELLER_STRUCTURE_EDITOR_H

