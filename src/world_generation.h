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

namespace godot {

class WorldGeneration : public Node2D {
    GDCLASS(WorldGeneration, Node2D)

private:
    // Constants
    static const int TILE_SIZE = 12;
    static const int WORLD_BUBBLE_SIZE = 64;
    static const int WORLD_BUBBLE_RADIUS = WORLD_BUBBLE_SIZE / 2;
    static const int TILE_Y_GROUND = 27;
    static const int TILE_Y_WALL = 53;

    // Internal data - stored as C++ containers for speed
    std::unordered_map<uint64_t, RID> tile_rids;           // tileOffset -> RID
    std::unordered_map<uint64_t, int> tile_y_cache;        // cellPos -> tile_y
    std::unordered_set<uint64_t> seen_cells;               // cells that have been seen
    
    // References set from GDScript
    Ref<FastNoiseLite> biome_noise;
    Ref<Texture2D> tilesheet;
    int world_seed = 0;
    
    // Helpers
    static inline uint64_t pack_coords(int x, int y) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | 
               static_cast<uint64_t>(static_cast<uint32_t>(y));
    }
    
    int get_tile_y(int x, int y);
    bool is_occluded_internal(const Vector2i& cellPos, const Vector2i& playerPos);

protected:
    static void _bind_methods();

public:
    WorldGeneration();
    ~WorldGeneration();
    
    // Expose constants to GDScript
    static int get_bubble_radius() { return WORLD_BUBBLE_RADIUS; }
    
    // Called from GDScript to set up resources
    void set_biome_noise(const Ref<FastNoiseLite>& noise);
    Ref<FastNoiseLite> get_biome_noise() const;
    void set_tilesheet(const Ref<Texture2D>& texture);
    Ref<Texture2D> get_tilesheet() const;
    void set_world_seed(int seed);
    int get_world_seed() const;
    
    // Main functions called from GDScript
    void init_world_bubble(const Vector2i& playerPos);
    void update_world_bubble(const Vector2i& playerPos);
};

}

#endif // ! SPACETRAVELLER_WORLD_GENERATION_H
