#ifndef SPACETRAVELLER_ENTITY_TRACKER_H
#define SPACETRAVELLER_ENTITY_TRACKER_H

#include <godot_cpp/variant/vector2i.hpp>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace godot {

class EntityPool;

class EntityTracker {
public:
    bool insert(uint32_t entity_id, const Vector2i& pos);
    bool move(uint32_t entity_id, const Vector2i& old_pos, const Vector2i& new_pos);
    void remove(uint32_t entity_id);
    void clear();

    bool contains(uint32_t entity_id) const;
    bool get_position(uint32_t entity_id, Vector2i& out_pos) const;
    uint32_t get_at(const Vector2i& pos) const;

    void query_rect(const Vector2i& min_pos, const Vector2i& max_pos, std::vector<uint32_t>& out_ids) const;
    void query_radius(const Vector2i& center, int radius, std::vector<uint32_t>& out_ids) const;
    void collect_ids(std::vector<uint32_t>& out_ids) const;

    void rebuild_from_pool(const EntityPool& pool);
    size_t size() const { return positions_by_entity.size(); }

private:
    std::unordered_map<uint32_t, Vector2i> positions_by_entity;
    std::unordered_map<uint64_t, uint32_t> entities_by_cell;
    std::unordered_map<uint64_t, std::unordered_set<uint32_t>> entities_by_bucket;

    static uint64_t cell_key(const Vector2i& pos);
    static uint64_t bucket_key(const Vector2i& pos);
    static Vector2i bucket_pos(const Vector2i& pos);

    void add_to_bucket(uint32_t entity_id, const Vector2i& pos);
    void remove_from_bucket(uint32_t entity_id, const Vector2i& pos);
};

}

#endif // SPACETRAVELLER_ENTITY_TRACKER_H
