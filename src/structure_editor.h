#ifndef SPACETRAVELLER_STRUCTURE_EDITOR_H
#define SPACETRAVELLER_STRUCTURE_EDITOR_H

#include "fast_tilemap.h"

namespace godot {

class StructureEditor : public Node2D {
    GDCLASS(StructureEditor, Node2D)

public:
    enum ShapeType {
        SHAPE_RECTANGLE,
        SHAPE_ELLIPSIS
    };

protected:
    static void _bind_methods();

    FastTileMap *tilemap = nullptr;
    std::unordered_map<uint64_t, RID> preview_tile_rids;
    
    void _create_preview_tile(const Vector2i &p_pos, const String &p_tile_id, RenderingServer *p_rs, RID p_texture_rid, RID p_parent_rid, int p_half, int p_cell_size, TileDb *p_tile_db);
    std::vector<Vector2i> _get_shape_points(ShapeType p_type, const Vector2i &p_p1, const Vector2i &p_p2, bool p_filled, bool p_perfect);

public:
    StructureEditor();
    ~StructureEditor();

    void set_tilemap(FastTileMap *p_tilemap);
    FastTileMap *get_tilemap() const;

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

