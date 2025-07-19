#ifndef SPACETRAVELLER_WORLD_GENERATION_H
#define SPACETRAVELLER_WORLD_GENERATION_H

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace godot {

class WorldGeneration: public Node2D{
    GDCLASS(WorldGeneration, Node2D)

private:
    bool is_occluded(const Vector2i& cellPos, const Vector2i& playerPos, const Dictionary& tileData);

protected:
	static void _bind_methods();

public:
    WorldGeneration();
    ~WorldGeneration();
};

}

#endif // ! SPACETRAVELLER_WORLD_GENERATION_H