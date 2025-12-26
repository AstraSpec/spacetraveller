#ifndef SPACETRAVELLER_STRUCTURE_EDITOR_H
#define SPACETRAVELLER_STRUCTURE_EDITOR_H

#include "fast_tilemap.h"

namespace godot {

class StructureEditor : public FastTileMap {
    GDCLASS(StructureEditor, FastTileMap)

protected:
    static void _bind_methods();

public:
    StructureEditor();
    ~StructureEditor();

    void update_visuals(const Vector2i& centerPos);
    Dictionary export_to_rle(const String &p_id) const;
};

}

#endif // SPACETRAVELLER_STRUCTURE_EDITOR_H

