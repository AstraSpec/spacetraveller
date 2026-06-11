#include "world_bubble.h"
#include "entities/entity_pool.h"
#include "core/id_registry.h"
#include "data/tile_db.h"
#include "core/world_coords.h"
#include "occlusion.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <queue>

using namespace godot;

uint16_t WorldBubble::resolve_tile_id(int layer, uint64_t cell_key, int world_x, int world_y) {
    auto override_it = tile_overrides[layer].find(cell_key);
    if (override_it != tile_overrides[layer].end()) {
        return override_it->second;
    }

    auto it = generated_tile_cache[layer].find(cell_key);
    if (it != generated_tile_cache[layer].end()) {
        return it->second;
    }
    if (layer == LAYER_TILE && tile_source) {
        uint16_t tile_id = tile_source(world_x, world_y);
        if (tile_id != 0) {
            generated_tile_cache[layer][cell_key] = tile_id;
            return tile_id;
        }
    }
    return 0;
}

static bool is_adjacent_to_player(int ox, int oy) {
    return ox >= -1 && ox <= 1 && oy >= -1 && oy <= 1 && !(ox == 0 && oy == 0);
}

void WorldBubble::place_tile(int x, int y, const String& tile_id, Layer p_layer) {
    uint64_t cell_key = WorldCoords::pack_coords(x, y);
    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (id_reg) {
        tile_overrides[p_layer][cell_key] = id_reg->get_id(tile_id);
    }
}

void WorldBubble::place_tile_id(int x, int y, uint16_t tile_id, Layer p_layer) {
    tile_overrides[p_layer][WorldCoords::pack_coords(x, y)] = tile_id;
}

String WorldBubble::get_tile_at(int x, int y, Layer p_layer) const {
    uint64_t cell_key = WorldCoords::pack_coords(x, y);
    uint16_t tile_id = 0;
    bool found = false;
    auto override_it = tile_overrides[p_layer].find(cell_key);
    if (override_it != tile_overrides[p_layer].end()) {
        tile_id = override_it->second;
        found = true;
    } else {
        auto generated_it = generated_tile_cache[p_layer].find(cell_key);
        if (generated_it != generated_tile_cache[p_layer].end()) {
            tile_id = generated_it->second;
            found = true;
        }
    }
    if (found) {
        IdRegistry* id_reg = IdRegistry::get_singleton();
        if (id_reg) {
            return id_reg->get_string(tile_id);
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
    target_id = resolve_tile_id(p_layer, start_key, x, y);

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
            uint16_t current_id = resolve_tile_id(p_layer, key, p.x, p.y);

            if (current_id == target_id) {
                tile_overrides[p_layer][key] = new_id;
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
                uint16_t current_id = resolve_tile_id(p_layer, key, gx, gy);

                if (current_id == target_id) {
                    tile_overrides[p_layer][key] = new_id;
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

void WorldBubble::set_tile_metadata(const Vector2i& pos, const Dictionary& data) {
    uint64_t key = WorldCoords::pack_coords(pos.x, pos.y);
    tile_metadata[key] = data;
}

Dictionary WorldBubble::get_tile_metadata(const Vector2i& pos) const {
    uint64_t key = WorldCoords::pack_coords(pos.x, pos.y);
    auto it = tile_metadata.find(key);
    return it != tile_metadata.end() ? it->second : Dictionary();
}

void WorldBubble::clear_tile_metadata(const Vector2i& pos) {
    uint64_t key = WorldCoords::pack_coords(pos.x, pos.y);
    tile_metadata.erase(key);
}

void WorldBubble::clear_all_tile_metadata() {
    tile_metadata.clear();
}

Dictionary WorldBubble::serialize_tile_metadata() const {
    Dictionary data;
    for (const auto& [key, metadata] : tile_metadata) {
        data[static_cast<int64_t>(key)] = metadata;
    }
    return data;
}

void WorldBubble::deserialize_tile_metadata(const Dictionary& data) {
    tile_metadata.clear();
    Array keys = data.keys();
    for (int i = 0; i < keys.size(); i++) {
        Variant key_var = keys[i];
        uint64_t key;
        if (key_var.get_type() == Variant::STRING) {
            key = static_cast<uint64_t>(((String)key_var).to_int());
        } else {
            key = static_cast<uint64_t>(static_cast<int64_t>(key_var));
        }

        Variant value = data[key_var];
        if (value.get_type() == Variant::DICTIONARY) {
            tile_metadata[key] = value;
        }
    }
}

Dictionary WorldBubble::serialize_ground_items() const {
    return cell_data.serialize();
}

void WorldBubble::deserialize_ground_items(const Dictionary& data) {
    cell_data.deserialize(data);
}

void WorldBubble::invalidate_tile_cache(int world_x, int world_y, Layer p_layer) {
    uint64_t key = WorldCoords::pack_coords(world_x, world_y);
    tile_overrides[p_layer].erase(key);
    generated_tile_cache[p_layer].erase(key);
}

void WorldBubble::invalidate_region_cache(const Rect2i& p_rect, Layer p_layer) {
    auto erase_region = [&](std::unordered_map<uint64_t, uint16_t>& cache) {
        for (auto it = cache.begin(); it != cache.end(); ) {
            Vector2i pos = WorldCoords::unpack_coords(it->first);
            if (p_rect.has_point(pos)) {
                it = cache.erase(it);
            } else {
                ++it;
            }
        }
    };
    erase_region(tile_overrides[p_layer]);
    erase_region(generated_tile_cache[p_layer]);
}

void WorldBubble::clear_cache(Layer p_layer) {
    tile_overrides[p_layer].clear();
    generated_tile_cache[p_layer].clear();
}

void WorldBubble::clear_all_caches() {
    for (int l = 0; l < LAYER_MAX; l++) {
        tile_overrides[l].clear();
        generated_tile_cache[l].clear();
    }
}

bool WorldBubble::set_entity(int x, int y, uint32_t entity_id) {
    uint64_t key = WorldCoords::pack_coords(x, y);
    auto it = entity_positions.find(key);
    if (it != entity_positions.end() && it->second.entity_id != entity_id) {
        if (!entity_pool_source || entity_pool_source->contains(it->second.entity_id)) {
            UtilityFunctions::printerr(
                String("[WorldBubble] Refusing to place entity ")
                + String::num_int64(entity_id)
                + String(" on occupied cell ")
                + String::num_int64(x)
                + String(",")
                + String::num_int64(y)
                + String(" occupied_by=")
                + String::num_int64(it->second.entity_id)
            );
            return false;
        }
    }
    entity_positions[key] = {entity_id};
    return true;
}

void WorldBubble::force_set_entity(int x, int y, uint32_t entity_id) {
    uint64_t key = WorldCoords::pack_coords(x, y);
    entity_positions[key] = {entity_id};
}

void WorldBubble::remove_entity(int x, int y) {
    uint64_t key = WorldCoords::pack_coords(x, y);
    entity_positions.erase(key);
}

bool WorldBubble::update_entity_position(int old_x, int old_y, int new_x, int new_y, uint32_t entity_id) {
    uint64_t new_key = WorldCoords::pack_coords(new_x, new_y);
    auto dest_it = entity_positions.find(new_key);
    if (dest_it != entity_positions.end() && dest_it->second.entity_id != entity_id) {
        if (!entity_pool_source || entity_pool_source->contains(dest_it->second.entity_id)) {
            UtilityFunctions::printerr(
                String("[WorldBubble] Refusing to move entity ")
                + String::num_int64(entity_id)
                + String(" into occupied cell ")
                + String::num_int64(new_x)
                + String(",")
                + String::num_int64(new_y)
                + String(" occupied_by=")
                + String::num_int64(dest_it->second.entity_id)
            );
            return false;
        }
    }

    uint64_t old_key = WorldCoords::pack_coords(old_x, old_y);
    auto it = entity_positions.find(old_key);
    if (it != entity_positions.end() && it->second.entity_id == entity_id) {
        entity_positions.erase(it);
    }
    entity_positions[new_key] = {entity_id};
    return true;
}

void WorldBubble::clear_entities() {
    entity_positions.clear();
}

void WorldBubble::rebuild_from_pool() {
    clear_entities();
    if (!entity_pool_source) {
        return;
    }
    for (uint32_t id : entity_pool_source->get_live_ids()) {
        const Entity* e = entity_pool_source->get_entity(id);
        if (!e) continue;
        force_set_entity(e->x, e->y, id);
    }
}

const WorldBubble::CellEntity* WorldBubble::get_entity_at(int x, int y) const {
    uint64_t key = WorldCoords::pack_coords(x, y);
    auto it = entity_positions.find(key);
    if (it == entity_positions.end()) return nullptr;
    if (entity_pool_source && !entity_pool_source->contains(it->second.entity_id)) return nullptr;
    return &it->second;
}

Dictionary WorldBubble::get_tile_id_cache(Layer p_layer) const {
    Dictionary d;
    for (auto const& [key, val] : tile_overrides[p_layer]) {
        d[key] = (int)val;
    }
    return d;
}

void WorldBubble::set_tile_id_cache(const Dictionary& p_cache, Layer p_layer) {
    tile_overrides[p_layer].clear();
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
        tile_overrides[p_layer][key] = (uint16_t)((int)p_cache[key_var]);
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

std::vector<uint64_t> WorldBubble::consume_newly_seen_cells() {
    std::vector<uint64_t> out = newly_seen_cells;
    newly_seen_cells.clear();
    return out;
}

uint16_t WorldBubble::query_tile_id(int x, int y) {
    uint64_t cell_key = WorldCoords::pack_coords(x, y);
    return resolve_tile_id(LAYER_TILE, cell_key, x, y);
}

void WorldBubble::add_overlay(int x, int y, uint16_t atlas_x, uint16_t atlas_y, const Color& color, float lifetime) {
    uint64_t key = WorldCoords::pack_coords(x, y);
    Overlay ov;
    ov.atlas_x = atlas_x;
    ov.atlas_y = atlas_y;
    ov.color = color;
    ov.lifetime = lifetime;
    ov.age = 0.0f;
    overlays[key] = ov;
}

void WorldBubble::remove_overlay(int x, int y) {
    overlays.erase(WorldCoords::pack_coords(x, y));
}

void WorldBubble::clear_overlays() {
    overlays.clear();
}

bool WorldBubble::tick_overlays(float delta) {
    bool changed = false;
    for (auto it = overlays.begin(); it != overlays.end(); ) {
        if (it->second.lifetime >= 0.0f) {
            it->second.age += delta;
            if (it->second.age >= it->second.lifetime) {
                it = overlays.erase(it);
                changed = true;
                continue;
            }
            changed = true;
        }
        ++it;
    }
    return changed;
}

bool WorldBubble::has_timed_overlays() const {
    for (const auto& [key, ov] : overlays) {
        if (ov.lifetime >= 0.0f) return true;
    }
    return false;
}

TraversalSnapshot WorldBubble::build_traversal_snapshot(
    const Vector2i& start,
    const Vector2i& goal,
    const std::vector<Vector2i>& blocking_positions,
    const EntityLedger* ledger,
    uint32_t entity_id,
    const String& traversal_profile,
    bool allow_openable_tiles
) {
    return TraversalSnapshot(this, start, goal, blocking_positions, ledger, entity_id, traversal_profile, allow_openable_tiles);
}

void WorldBubble::update_visibility(
    const Vector2i& player_pos,
    const std::vector<uint64_t>& offset_keys,
    bool occlusion_enabled
) {
    visible_cells.clear();

    if (!occlusion_enabled) {
        for (uint64_t offset_key : offset_keys) {
            Vector2i offset = WorldCoords::unpack_coords(offset_key);
            int cx = offset.x + player_pos.x;
            int cy = offset.y + player_pos.y;
            uint64_t cell_key = WorldCoords::pack_coords(cx, cy);
            resolve_tile_id(LAYER_TILE, cell_key, cx, cy);
            visible_cells.insert(cell_key);
        }
        return;
    }

    std::unordered_map<uint64_t, uint16_t> visibility_tiles;
    visibility_tiles.reserve(offset_keys.size());
    for (uint64_t offset_key : offset_keys) {
        Vector2i offset = WorldCoords::unpack_coords(offset_key);
        int cx = offset.x + player_pos.x;
        int cy = offset.y + player_pos.y;
        uint64_t cell_key = WorldCoords::pack_coords(cx, cy);
        visibility_tiles[cell_key] = resolve_tile_id(LAYER_TILE, cell_key, cx, cy);
    }

    Occlusion::compute_visible(player_pos, world_bubble_radius, visibility_tiles, visible_cells);
    for (uint64_t cell_key : visible_cells) {
        if (seen_cells.insert(cell_key).second) {
            newly_seen_cells.push_back(cell_key);
        }
    }
}

WorldBubble::BubbleSnapshot WorldBubble::build_snapshot(
    const Vector2i& player_pos,
    const std::vector<uint64_t>& offset_keys,
    bool occlusion_enabled
) {
    BubbleSnapshot snapshot;

    static const bool LAYER_HAS_ITEMS[LAYER_MAX] = { true, false };
    TileDb* tile_db = TileDb::get_singleton();

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
            }

            visual.tile_id = resolve_tile_id(l, cell_key, cx, cy);

            if (LAYER_HAS_ITEMS[l]) {
                const DroppedItem* top = cell_data.get_top_item(cell_key);
                bool item_hidden_by_tile = tile_db && tile_db->hides_items_at(visual.tile_id) && !is_adjacent_to_player(ox, oy);
                if (top && !item_hidden_by_tile) {
                    visual.draw_item = true;
                    visual.item_id = top->id;

                    auto ent_it = entity_positions.find(cell_key);
                    if (ent_it != entity_positions.end()) {
                        visual.entity_sprite_id = 1;
                        if (entity_pool_source) {
                            const Entity* entity = entity_pool_source->get_entity(ent_it->second.entity_id);
                            if (entity) {
                                visual.entity_atlas_x = entity->atlas_x;
                                visual.entity_atlas_y = entity->atlas_y;
                            }
                        }
                    }

                    snapshot.cells[l][offset_key] = visual;
                    continue;
                }
            }

            auto ent_it = entity_positions.find(cell_key);
            if (ent_it != entity_positions.end()) {
                visual.entity_sprite_id = 1;
                if (entity_pool_source) {
                    const Entity* entity = entity_pool_source->get_entity(ent_it->second.entity_id);
                    if (entity) {
                        visual.entity_atlas_x = entity->atlas_x;
                        visual.entity_atlas_y = entity->atlas_y;
                    }
                }
            }

            if (l == LAYER_INDICATOR) {
                auto ov_it = overlays.find(cell_key);
                if (ov_it != overlays.end()) {
                    const Overlay& ov = ov_it->second;
                    visual.draw_overlay = true;
                    visual.overlay_atlas_x = ov.atlas_x;
                    visual.overlay_atlas_y = ov.atlas_y;
                    visual.overlay_color = ov.color;
                    if (ov.lifetime > 0.0f) {
                        float remaining = 1.0f - (ov.age / ov.lifetime);
                        if (remaining < 0.0f) remaining = 0.0f;
                        visual.overlay_color.a *= remaining;
                    }
                }
            }

            snapshot.cells[l][offset_key] = visual;
        }
    }

    return snapshot;
}
