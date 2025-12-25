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

namespace godot {

class WorldGeneration : public Node2D {
    GDCLASS(WorldGeneration, Node2D)

private:
    // Constants
    static const int TILE_SIZE = 12;
    static const int WORLD_BUBBLE_SIZE = 64;
    static const int WORLD_BUBBLE_RADIUS = WORLD_BUBBLE_SIZE / 2;
    static const int REGION_SIZE = 256;
    static const int CHUNK_SIZE = 32;
    static const int CHUNK_SHIFT = 4;
    
    std::unordered_map<uint64_t, String> region_chunks;
    std::unordered_map<uint64_t, RID> tile_rids;
    std::unordered_map<uint64_t, String> tile_id_cache;
    std::unordered_set<uint64_t> seen_cells;
    
    // References set from GDScript
    Ref<FastNoiseLite> biome_noise;
    Ref<Texture2D> tilesheet;
    int world_seed = 0;
    
    // Helpers
    String get_tile(int x, int y);

protected:
    static void _bind_methods();

public:
    WorldGeneration();
    ~WorldGeneration();
    
    static int get_bubble_radius() { return WORLD_BUBBLE_RADIUS; }
    
    void set_biome_noise(const Ref<FastNoiseLite>& noise);
    Ref<FastNoiseLite> get_biome_noise() const;
    void set_tilesheet(const Ref<Texture2D>& texture);
    Ref<Texture2D> get_tilesheet() const;
    void set_world_seed(int seed);
    int get_world_seed() const;
    
    void init_world_bubble(const Vector2i& playerPos);
    void update_world_bubble(const Vector2i& playerPos);
    Dictionary init_region(const Vector2i& regionPos);
};

}

#endif // ! SPACETRAVELLER_WORLD_GENERATION_H
