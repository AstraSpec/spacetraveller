#include "world_bubble.h"
#include "core/id_registry.h"
#include "core/world_coords.h"
#include "occlusion.h"
#include <queue>

using namespace godot;

uint16_t WorldBubble::resolve_tile_id(int layer, uint64_t cell_key, int world_x, int world_y) {
    auto it = tile_id_cache[layer].find(cell_key);
    if (it != tile_id_cache[layer].end()) {
        return it->second;
    }
    if (layer == LAYER_TILE && tile_source) {
        uint16_t tile_id = tile_source(world_x, world_y);
        if (tile_id != 0) {
            tile_id_cache[layer][cell_key] = tile_id;
            return tile_id;
        }
    }
    return 0;
}

void WorldBubble::place_tile(int x, int y, const String& tile_id, Layer p_layer) {
    uint64_t cell_key = WorldCoords::pack_coords(x, y);
    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (id_reg) {
        tile_id_cache[p_layer][cell_key] = id_reg->get_id(tile_id);
    }
}

String WorldBubble::get_tile_at(int x, int y, Layer p_layer) const {
    uint64_t cell_key = WorldCoords::pack_coords(x, y);
    auto it = tile_id_cache[p_layer].find(cell_key);
    if (it != tile_id_cache[p_layer].end()) {
        IdRegistry* id_reg = IdRegistry::get_singleton();
        if (id_reg) {
            return id_reg->get_string(it->second);
        }
    }
    return "void";
}

void WorldBubble::fill_tiles(int x, int y, const String& tile_id, const Vector2i& playerPos, const Rect2i& mask, bool invert_mask, bool p_contiguous, Layer p_layer) {
    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (!id_reg) return;

    uint16_t new_id = id_reg->get_id(tile_id);
    uint16_t target_id = 0;

    uint64_t start_key = WorldCoords::pack_coords(x, y);
    auto it = tile_id_cache[p_layer].find(start_key);
    if (it != tile_id_cache[p_layer].end()) {
        target_id = it->second;
    }

    if (new_id == target_id) return;

    int radius = world_bubble_radius;
    bool has_mask = mask.size.x > 0 && mask.size.y > 0;

    if (p_contiguous) {
        std::queue<Vector2i> q;
        q.push(Vector2i(x, y));

        while (!q.empty()) {
            Vector2i p = q.front();
            q.pop();

            if (p.x < playerPos.x - radius || p.x >= playerPos.x + radius ||
                p.y < playerPos.y - radius || p.y >= playerPos.y + radius) continue;

            if (has_mask) {
                Vector2i relative_p = p - playerPos;
                bool inside = mask.has_point(relative_p);
                if (invert_mask) {
                    if (inside) continue;
                } else {
                    if (!inside) continue;
                }
            }

            uint64_t key = WorldCoords::pack_coords(p.x, p.y);
            uint16_t current_id = 0;
            auto it_cur = tile_id_cache[p_layer].find(key);
            if (it_cur != tile_id_cache[p_layer].end()) {
                current_id = it_cur->second;
            }

            if (current_id == target_id) {
                tile_id_cache[p_layer][key] = new_id;
                q.push(Vector2i(p.x + 1, p.y));
                q.push(Vector2i(p.x - 1, p.y));
                q.push(Vector2i(p.x, p.y + 1));
                q.push(Vector2i(p.x, p.y - 1));
            }
        }
    } else {
        for (int gy = playerPos.y - radius; gy < playerPos.y + radius; gy++) {
            for (int gx = playerPos.x - radius; gx < playerPos.x + radius; gx++) {
                Vector2i p(gx, gy);

                if (has_mask) {
                    Vector2i relative_p = p - playerPos;
                    bool inside = mask.has_point(relative_p);
                    if (invert_mask) {
                        if (inside) continue;
                    } else {
                        if (!inside) continue;
                    }
                }

                uint64_t key = WorldCoords::pack_coords(gx, gy);
                uint16_t current_id = 0;
                auto it_cur = tile_id_cache[p_layer].find(key);
                if (it_cur != tile_id_cache[p_layer].end()) {
                    current_id = it_cur->second;
                }

                if (current_id == target_id) {
                    tile_id_cache[p_layer][key] = new_id;
                }
            }
        }
    }
}

const DroppedItem* WorldBubble::get_top_item(int x, int y) const {
    uint64_t cell_key = WorldCoords::pack_coords(x, y);
    return cell_data.get_top_item(cell_key);
}

void WorldBubble::drop_item(const Vector2i& pos, uint16_t item_id, int amount) {
    uint64_t key = WorldCoords::pack_coords(pos.x, pos.y);
    cell_data.add_item(key, item_id, amount);
}

int WorldBubble::remove_item(const Vector2i& pos, uint16_t item_id, int amount) {
    uint64_t key = WorldCoords::pack_coords(pos.x, pos.y);
    return cell_data.remove_item(key, item_id, amount);
}

int WorldBubble::peek_item_amount(const Vector2i& pos, uint16_t item_id) const {
    uint64_t key = WorldCoords::pack_coords(pos.x, pos.y);
    return cell_data.peek_item_amount(key, item_id);
}

Array WorldBubble::get_items_at(const Vector2i& pos) const {
    Array list;
    uint64_t key = WorldCoords::pack_coords(pos.x, pos.y);
    const std::vector<DroppedItem>* items = cell_data.get_items(key);
    if (!items) return list;

    IdRegistry* reg = IdRegistry::get_singleton();
    for (const auto& item : *items) {
        Dictionary d;
        d["id"] = reg ? reg->get_string(item.id) : String::num_int64(item.id);
        d["amount"] = item.amount;
        list.push_back(d);
    }
    return list;
}

bool WorldBubble::has_items(const Vector2i& pos) const {
    uint64_t key = WorldCoords::pack_coords(pos.x, pos.y);
    return cell_data.has_items(key);
}

Dictionary WorldBubble::serialize_ground_items() const {
    return cell_data.serialize();
}

void WorldBubble::deserialize_ground_items(const Dictionary& data) {
    cell_data.deserialize(data);
}

void WorldBubble::invalidate_tile_cache(int world_x, int world_y, Layer p_layer) {
    uint64_t key = WorldCoords::pack_coords(world_x, world_y);
    tile_id_cache[p_layer].erase(key);
}

void WorldBubble::invalidate_region_cache(const Rect2i& p_rect, Layer p_layer) {
    auto& cache = tile_id_cache[p_layer];
    for (auto it = cache.begin(); it != cache.end(); ) {
        Vector2i pos = WorldCoords::unpack_coords(it->first);
        if (p_rect.has_point(pos)) {
            it = cache.erase(it);
        } else {
            ++it;
        }
    }
}

void WorldBubble::clear_cache(Layer p_layer) {
    tile_id_cache[p_layer].clear();
}

void WorldBubble::clear_all_caches() {
    for (int l = 0; l < LAYER_MAX; l++) {
        tile_id_cache[l].clear();
    }
}

Dictionary WorldBubble::get_tile_id_cache(Layer p_layer) const {
    Dictionary d;
    for (auto const& [key, val] : tile_id_cache[p_layer]) {
        d[key] = (int)val;
    }
    return d;
}

void WorldBubble::set_tile_id_cache(const Dictionary& p_cache, Layer p_layer) {
    tile_id_cache[p_layer].clear();
    merge_tile_id_cache(p_cache, p_layer);
}

void WorldBubble::merge_tile_id_cache(const Dictionary& p_cache, Layer p_layer) {
    Array keys = p_cache.keys();
    for (int i = 0; i < keys.size(); i++) {
        Variant key_var = keys[i];
        uint64_t key;
        if (key_var.get_type() == Variant::STRING) {
            key = ((String)key_var).to_int();
        } else {
            key = key_var;
        }
        tile_id_cache[p_layer][key] = (uint16_t)((int)p_cache[key_var]);
    }
}

Array WorldBubble::get_seen_cells() const {
    Array a;
    for (uint64_t key : seen_cells) {
        a.push_back(key);
    }
    return a;
}

void WorldBubble::set_seen_cells(const Array& p_seen) {
    seen_cells.clear();
    for (int i = 0; i < p_seen.size(); i++) {
        Variant v = p_seen[i];
        if (v.get_type() == Variant::STRING) {
            seen_cells.insert(((String)v).to_int());
        } else {
            seen_cells.insert((uint64_t)v);
        }
    }
}

bool WorldBubble::is_cell_seen(int x, int y) const {
    uint64_t cell_key = WorldCoords::pack_coords(x, y);
    return seen_cells.count(cell_key) > 0;
}

uint16_t WorldBubble::query_tile_id(int x, int y) {
    uint64_t cell_key = WorldCoords::pack_coords(x, y);
    return resolve_tile_id(LAYER_TILE, cell_key, x, y);
}

TraversalSnapshot WorldBubble::build_traversal_snapshot(
    const Vector2i& start,
    const Vector2i& goal,
    const std::vector<Vector2i>& blocking_positions
) {
    return TraversalSnapshot(this, start, goal, blocking_positions);
}

WorldBubble::BubbleSnapshot WorldBubble::build_snapshot(
    const Vector2i& player_pos,
    const std::vector<uint64_t>& offset_keys,
    bool occlusion_enabled
) {
    BubbleSnapshot snapshot;

    visible_cells.clear();
    if (occlusion_enabled) {
        // Resolve tiles before FOV so procedural/cache-backed solids block sight on the first frame.
        for (uint64_t offset_key : offset_keys) {
            Vector2i offset = WorldCoords::unpack_coords(offset_key);
            int cx = offset.x + player_pos.x;
            int cy = offset.y + player_pos.y;
            uint64_t cell_key = WorldCoords::pack_coords(cx, cy);
            resolve_tile_id(LAYER_TILE, cell_key, cx, cy);
        }
        Occlusion::compute_visible(player_pos, world_bubble_radius, tile_id_cache[LAYER_TILE], visible_cells);
    }

    static const bool LAYER_HAS_ITEMS[LAYER_MAX] = { true, false };

    for (int l = 0; l < LAYER_MAX; l++) {
        for (uint64_t offset_key : offset_keys) {
            Vector2i offset = WorldCoords::unpack_coords(offset_key);
            int ox = offset.x;
            int oy = offset.y;
            int cx = ox + player_pos.x;
            int cy = oy + player_pos.y;
            uint64_t cell_key = WorldCoords::pack_coords(cx, cy);

            CellVisual visual;

            if (occlusion_enabled) {
                visual.occluded = visible_cells.find(cell_key) == visible_cells.end();
                visual.seen = seen_cells.count(cell_key) > 0;
                if (!visual.occluded) {
                    seen_cells.insert(cell_key);
                    visual.seen = true;
                }
            }

            if (LAYER_HAS_ITEMS[l]) {
                const DroppedItem* top = cell_data.get_top_item(cell_key);
                if (top) {
                    visual.draw_item = true;
                    visual.item_id = top->id;
                    snapshot.cells[l][offset_key] = visual;
                    continue;
                }
            }

            visual.tile_id = resolve_tile_id(l, cell_key, cx, cy);
            snapshot.cells[l][offset_key] = visual;
        }
    }

    return snapshot;
}
