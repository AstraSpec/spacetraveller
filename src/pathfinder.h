#ifndef SPACETRAVELLER_PATHFINDER_H
#define SPACETRAVELLER_PATHFINDER_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/array.hpp>

namespace godot {

class GameWorld;

class Pathfinder : public Object {
    GDCLASS(Pathfinder, Object)

protected:
    static void _bind_methods();

public:
    Pathfinder();
    ~Pathfinder();

    static Array find_path(GameWorld* p_world, const Vector2i& p_start, const Vector2i& p_end);
    static bool is_walkable(GameWorld* p_world, const Vector2i& p_pos);
};

}

#endif // SPACETRAVELLER_PATHFINDER_H
