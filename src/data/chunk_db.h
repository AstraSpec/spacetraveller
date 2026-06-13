#ifndef SPACETRAVELLER_CHUNK_DB_H
#define SPACETRAVELLER_CHUNK_DB_H

#include <godot_cpp/classes/object.hpp>
#include "database.h"

namespace godot {

struct ChunkFeatureSpawnInfo {
    String pool;
    String placement;
    float chance = 0.0f;
    int candidates = 0;
};

struct ChunkInfo {
    Vector2i atlas;
    std::vector<uint16_t> tags;
    int city_spawn_weight = 0;
    float wilderness_spawn_chance = 0.0f;
    String structure_type;
    String dungeon_type;
    std::vector<ChunkFeatureSpawnInfo> feature_spawns;
};

struct CityChunkSpawnInfo {
    uint16_t id = 0;
    int weight = 0;
};

class ChunkDb : public Object, public DataBase<ChunkInfo, ChunkDb> {
    GDCLASS(ChunkDb, Object)

private:
    std::vector<ChunkInfo> fast_cache;
    std::vector<CityChunkSpawnInfo> city_spawn_chunks;
    std::vector<CityChunkSpawnInfo> wilderness_spawn_chunks;
    int city_spawn_total_weight = 0;

protected:
    static void _bind_methods();
    virtual ChunkInfo _parse_row(const Dictionary &p_data) override;

public:
    ChunkDb();
    ~ChunkDb();

    void initialize_data();
    Array get_ids() const { return DataBase::get_ids(); }

    // Fast C++ access
    const ChunkInfo* get_chunk_info(const String &p_id) const;
    const ChunkInfo* get_chunk_info(uint16_t p_id) const;
    const std::vector<CityChunkSpawnInfo>& get_city_spawn_chunks() const { return city_spawn_chunks; }
    const std::vector<CityChunkSpawnInfo>& get_wilderness_spawn_chunks() const { return wilderness_spawn_chunks; }
    int get_city_spawn_total_weight() const { return city_spawn_total_weight; }
    bool has_tag(const String &p_id, const String &p_tag) const;
    bool has_tag(uint16_t p_id, uint16_t p_tag_id) const;

    // GDScript/Standard access
    Vector2i get_atlas_coords(const String &p_id) const;
    bool has_tag_gd(const String &p_id, const String &p_tag) const { return has_tag(p_id, p_tag); }
};

}

#endif // ! SPACETRAVELLER_CHUNK_DB_H
