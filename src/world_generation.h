#ifndef SPACETRAVELLER_WORLD_GENERATION_H
#define SPACETRAVELLER_WORLD_GENERATION_H

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <unordered_map>

namespace godot {

class WorldGeneration: public Node2D{
    GDCLASS(WorldGeneration, Node2D)

private:
    bool is_occluded_internal(const Vector2i& cellPos, const Vector2i& playerPos, 
                              const std::unordered_map<uint64_t, int>& tileMap);
    
    static inline uint64_t pack_coords(int x, int y) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | 
               static_cast<uint64_t>(static_cast<uint32_t>(y));
    }

protected:
	static void _bind_methods();

public:
    WorldGeneration();
    ~WorldGeneration();
    
    // Batch process occlusion for all tiles at once
    Dictionary compute_occlusion_batch(const Array& tileOffsets, const Vector2i& playerPos, const Dictionary& tileData);
};

}

#endif // ! SPACETRAVELLER_WORLD_GENERATION_H