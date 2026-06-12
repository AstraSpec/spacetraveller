#ifndef SPACETRAVELLER_FAST_MAP_RENDERER_H
#define SPACETRAVELLER_FAST_MAP_RENDERER_H

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <unordered_map>
#include "core/world_coords.h"

namespace godot {

class FastMapRenderer : public Node2D {
    GDCLASS(FastMapRenderer, Node2D)

private:
    static const int TILE_SIZE = 12;

    Ref<Texture2D> tilesheet;
    std::unordered_map<uint64_t, RID> cell_rids;

protected:
    static void _bind_methods();

public:
    FastMapRenderer();
    ~FastMapRenderer();

    void set_tilesheet(const Ref<Texture2D>& p_texture);
    Ref<Texture2D> get_tilesheet() const;

    void clear();
    void set_cell(const Vector2i& p_pos, const Vector2i& p_atlas);

    static int get_tile_size() { return TILE_SIZE; }
};

}

#endif // SPACETRAVELLER_FAST_MAP_RENDERER_H
