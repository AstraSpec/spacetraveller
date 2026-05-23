#ifndef SPACETRAVELLER_GAME_WORLD_H
#define SPACETRAVELLER_GAME_WORLD_H

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/rect2i.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <cmath>
#include "core/world_coords.h"
#include "city_generation.h"
#include "data/tile_db.h"
#include "data/chunk_db.h"
#include "data/item_db.h"
#include "components/inventory.h"
#include "cell_data.h"
#include "fast_tilemap.h"
#include "world_generator.h"

namespace godot {

class GameWorld : public Node2D {
    GDCLASS(GameWorld, Node2D)

private:
    FastTileMap* renderer = nullptr;
    std::unique_ptr<CellData> cell_data;
    std::unique_ptr<WorldGenerator> generator;
    
    // References set from GDScript
    Ref<FastNoiseLite> biome_noise;
    int world_seed = 0;

protected:
    static void _bind_methods();

public:
    GameWorld();
    ~GameWorld();

    void setup_renderer();
    FastTileMap* get_renderer() const { return renderer; }
    
    static int get_region_size() { return WorldCoords::REGION_SIZE; }
    static int get_chunk_size() { return WorldCoords::CHUNK_SIZE; }

    static uint64_t pack_coords(int x, int y) { return WorldCoords::pack_coords(x, y); }
    static Vector2i unpack_coords(uint64_t key) { return WorldCoords::unpack_coords(key); }
    
    void set_biome_noise(const Ref<FastNoiseLite>& noise);
    Ref<FastNoiseLite> get_biome_noise() const;
    void set_world_seed(int seed);
    int get_world_seed() const;
    
    void update_world_bubble(const Vector2i& playerPos);
    Dictionary init_region(const Vector2i& regionPos);
    void drop_item(const Vector2i& pos, const String& item_id, int amount);
    Array get_items_at(const Vector2i& pos) const;
    bool pickup_item_specific(const Vector2i& pos, const String& item_id, int amount, Inventory* p_inventory);
    bool has_item(const Vector2i& pos) const;

    Dictionary get_save_data() const;
    void load_save_data(const Dictionary &p_data);
};

}

#endif // ! SPACETRAVELLER_GAME_WORLD_H
