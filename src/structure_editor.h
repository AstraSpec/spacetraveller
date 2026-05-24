#ifndef SPACETRAVELLER_STRUCTURE_EDITOR_H
#define SPACETRAVELLER_STRUCTURE_EDITOR_H

#include "world/game_world.h"
#include "world/fast_tilemap.h"

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

    GameWorld *world = nullptr;
    FastTileMap *tilemap = nullptr;
    std::unordered_map<uint64_t, RID> preview_tile_rids;

    void _create_preview_tile(const Vector2i &p_pos, const String &p_id, const String &p_entry_type, RenderingServer *p_rs, RID p_texture_rid, RID p_parent_rid, int p_half, int p_cell_size);
    std::vector<Vector2i> _get_shape_points(ShapeType p_type, const Vector2i &p_p1, const Vector2i &p_p2, bool p_filled, bool p_perfect);

public:
    StructureEditor();
    ~StructureEditor();

    void set_world(GameWorld *p_world);
    GameWorld *get_world() const;

    void set_tilemap(FastTileMap *p_tilemap);
    FastTileMap *get_tilemap() const;

    Dictionary export_to_rle(const String &p_id, const Vector2i &p_offset = Vector2i()) const;
    void import_from_rle(const String &p_blueprint, const Array &p_palette, const Vector2i &p_offset = Vector2i());

    void update_preview_tiles(const Array &p_positions, const String &p_tile_id, const String &p_entry_type = "tile");
    void update_preview_tiles_with_data(const Dictionary &p_data, const String &p_entry_type = "tile");
    void update_preview_shape(ShapeType p_type, const Vector2i &p_p1, const Vector2i &p_p2, bool p_filled, bool p_perfect, const String &p_tile_id, const String &p_entry_type = "tile");
    Array get_shape_points(ShapeType p_type, const Vector2i &p_p1, const Vector2i &p_p2, bool p_filled, bool p_perfect);
    void clear_preview_tiles();
};

}

VARIANT_ENUM_CAST(StructureEditor::ShapeType);

#endif // SPACETRAVELLER_STRUCTURE_EDITOR_H
