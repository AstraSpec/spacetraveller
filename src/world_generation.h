#ifndef SPACETRAVELLER_WORLD_GENERATION_H
#define SPACETRAVELLER_WORLD_GENERATION_H

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/rect2i.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cmath>
#include <godot_cpp/variant/utility_functions.hpp>
#include "occlusion.h"
#include "city_generation.h"
#include "data/tile_db.h"
#include "data/chunk_db.h"

#include "fast_tilemap.h"

namespace godot {

class WorldGeneration : public FastTileMap {
    GDCLASS(WorldGeneration, FastTileMap)

private:
    // Constants
    static const int REGION_SIZE = 256;
    static const int CHUNK_SIZE = 32;
    static const int CHUNK_SHIFT = 5;
    
    std::unordered_map<uint64_t, String> region_chunks;
    
    // References set from GDScript
    Ref<FastNoiseLite> biome_noise;
    int world_seed = 0;
    
    // Helpers
    String get_tile(int x, int y);

protected:
    static void _bind_methods();

public:
    WorldGeneration();
    ~WorldGeneration();
    
    static int get_region_size() { return REGION_SIZE; }
    static int get_chunk_size() { return CHUNK_SIZE; }
    static int get_chunk_shift() { return CHUNK_SHIFT; }
    
    void set_biome_noise(const Ref<FastNoiseLite>& noise);
    Ref<FastNoiseLite> get_biome_noise() const;
    void set_world_seed(int seed);
    int get_world_seed() const;
    
    void update_world_bubble(const Vector2i& playerPos);
    Dictionary init_region(const Vector2i& regionPos);
};

}

#endif // ! SPACETRAVELLER_WORLD_GENERATION_H
