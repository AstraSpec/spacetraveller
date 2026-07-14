#include "world_bubble.h"
#include "components/equipment.h"
#include "entities/entity_pool.h"
#include "entities/entity_ledger.h"
#include "core/id_registry.h"
#include "data/item_db.h"
#include "data/race_db.h"
#include "data/tile_db.h"
#include "core/world_coords.h"
#include "light_map.h"
#include "occlusion.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <queue>
#include <utility>

using namespace godot;

static constexpr int VERTICAL_AIR_VISIBILITY_DEPTH = 4;

uint64_t WorldBubble::make_cell_key(int world_x, int world_y) const {
    return make_cell_key_at_z(world_x, world_y, active_z);
}

uint64_t WorldBubble::make_cell_key_at_z(int world_x, int world_y, int world_z) const {
    return WorldCoords::pack_coords_3d(world_x, world_y, world_z);
}

uint16_t WorldBubble::resolve_tile_id(int layer, uint64_t cell_key, int world_x, int world_y, int world_z) {
    auto override_it = tile_overrides[layer].find(cell_key);
    if (override_it != tile_overrides[layer].end()) {
        return override_it->second;
    }

    auto it = generated_tile_cache[layer].find(cell_key);
    if (it != generated_tile_cache[layer].end()) {
        return it->second;
    }
    if (layer == LAYER_TILE && tile_source) {
        uint16_t tile_id = tile_source(world_x, world_y, world_z);
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

static bool tile_is_air(uint16_t tile_id) {
    IdRegistry* id_reg = IdRegistry::get_singleton();
    return id_reg && tile_id == id_reg->get_id("air");
}

static bool tile_allows_sky(uint16_t tile_id, const TileInfo* p_info) {
    if (tile_id == 0) {
        return true;
    }

    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (id_reg && (tile_id == id_reg->get_id("void") || tile_id == id_reg->get_id("air"))) {
        return true;
    }

    return p_info && p_info->transparent;
}

static bool tile_is_opaque(const TileInfo* p_info) {
    return p_info && p_info->solid && !p_info->transparent;
}

static void append_light_emitter(std::vector<LightEmitter>& r_emitters, const Vector2i& p_position, int p_z, LightStrength p_strength) {
    if (p_strength <= LIGHT_STRENGTH_BLANK) {
        return;
    }

    LightEmitter emitter;
    emitter.position = p_position;
    emitter.z = p_z;
    emitter.strength = p_strength;
    r_emitters.push_back(emitter);
}

static LightStrength item_light_strength_by_id(ItemDb* p_item_db, uint16_t p_item_id) {
    const ItemInfo* item = p_item_db ? p_item_db->get_item_info(p_item_id) : nullptr;
    return item ? item->light.strength : LIGHT_STRENGTH_BLANK;
}

static LightStrength item_light_strength_by_string(ItemDb* p_item_db, const String& p_item_id) {
    const ItemInfo* item = p_item_db ? p_item_db->get_item_info(p_item_id) : nullptr;
    return item ? item->light.strength : LIGHT_STRENGTH_BLANK;
}

static bool player_minimum_light_contains_offset(int p_ox, int p_oy, int p_radius) {
    if (p_radius <= 0) {
        return p_ox == 0 && p_oy == 0;
    }

    const int distance_sq = p_ox * p_ox + p_oy * p_oy;
    return distance_sq <= p_radius * p_radius + p_radius;
}

static void raise_view_facing_faces(
    LightSample& p_sample,
    const Vector2i& p_view_origin,
    const Vector2i& p_cell_pos,
    LightStrength p_strength
) {
    const int dx = p_view_origin.x - p_cell_pos.x;
    const int dy = p_view_origin.y - p_cell_pos.y;
    bool raised = false;

    if (dx > 0) {
        p_sample.raise_face(LightFace::East, p_strength);
        raised = true;
    } else if (dx < 0) {
        p_sample.raise_face(LightFace::West, p_strength);
        raised = true;
    }

    if (dy > 0) {
        p_sample.raise_face(LightFace::South, p_strength);
        raised = true;
    } else if (dy < 0) {
        p_sample.raise_face(LightFace::North, p_strength);
        raised = true;
    }

    if (!raised) {
        p_sample.raise_all_faces(p_strength);
    }
}

static LightStrength view_facing_light(
    const LightSample& p_sample,
    const Vector2i& p_view_origin,
    const Vector2i& p_cell_pos
) {
    const int dx = p_view_origin.x - p_cell_pos.x;
    const int dy = p_view_origin.y - p_cell_pos.y;
    LightStrength strength = LIGHT_STRENGTH_BLANK;
    bool sampled = false;

    if (dx > 0) {
        strength = light_strength_stronger(strength, p_sample.get_face(LightFace::East));
        sampled = true;
    } else if (dx < 0) {
        strength = light_strength_stronger(strength, p_sample.get_face(LightFace::West));
        sampled = true;
    }

    if (dy > 0) {
        strength = light_strength_stronger(strength, p_sample.get_face(LightFace::South));
        sampled = true;
    } else if (dy < 0) {
        strength = light_strength_stronger(strength, p_sample.get_face(LightFace::North));
        sampled = true;
    }

    return sampled ? strength : p_sample.strongest_face();
}

void WorldBubble::place_tile(int x, int y, const String& tile_id, Layer p_layer) {
    uint64_t cell_key = make_cell_key(x, y);
    IdRegistry* id_reg = IdRegistry::get_singleton();
    if (id_reg) {
        tile_overrides[p_layer][cell_key] = id_reg->get_id(tile_id);
    }
}

void WorldBubble::place_tile_id(int x, int y, uint16_t tile_id, Layer p_layer) {
    tile_overrides[p_layer][make_cell_key(x, y)] = tile_id;
}

String WorldBubble::get_tile_at(int x, int y, Layer p_layer) const {
    uint64_t cell_key = make_cell_key(x, y);
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

    uint64_t start_key = make_cell_key(x, y);
    target_id = resolve_tile_id(p_layer, start_key, x, y, active_z);

    if (new_id == target_id) return;

    int radius = active_radius;
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

            uint64_t key = make_cell_key(p.x, p.y);
            uint16_t current_id = resolve_tile_id(p_layer, key, p.x, p.y, active_z);

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

                uint64_t key = make_cell_key(gx, gy);
                uint16_t current_id = resolve_tile_id(p_layer, key, gx, gy, active_z);

                if (current_id == target_id) {
                    tile_overrides[p_layer][key] = new_id;
                }
            }
        }
    }
}

const DroppedItem* WorldBubble::get_top_item(int x, int y) const {
    uint64_t cell_key = make_cell_key(x, y);
    return cell_data.get_top_item(cell_key);
}

void WorldBubble::drop_item(const Vector2i& pos, uint16_t item_id, int amount) {
    uint64_t key = make_cell_key(pos.x, pos.y);
    cell_data.add_item(key, item_id, amount);
}

int WorldBubble::remove_item(const Vector2i& pos, uint16_t item_id, int amount) {
    uint64_t key = make_cell_key(pos.x, pos.y);
    return cell_data.remove_item(key, item_id, amount);
}

int WorldBubble::peek_item_amount(const Vector2i& pos, uint16_t item_id) const {
    uint64_t key = make_cell_key(pos.x, pos.y);
    return cell_data.peek_item_amount(key, item_id);
}

Array WorldBubble::get_items_at(const Vector2i& pos) const {
    Array list;
    uint64_t key = make_cell_key(pos.x, pos.y);
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
    uint64_t key = make_cell_key(pos.x, pos.y);
    return cell_data.has_items(key);
}

void WorldBubble::set_tile_metadata(const Vector2i& pos, const Dictionary& data) {
    uint64_t key = make_cell_key(pos.x, pos.y);
    tile_metadata[key] = data;
}

Dictionary WorldBubble::get_tile_metadata(const Vector2i& pos) const {
    uint64_t key = make_cell_key(pos.x, pos.y);
    auto it = tile_metadata.find(key);
    return it != tile_metadata.end() ? it->second : Dictionary();
}

void WorldBubble::clear_tile_metadata(const Vector2i& pos) {
    uint64_t key = make_cell_key(pos.x, pos.y);
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
    uint64_t key = make_cell_key(world_x, world_y);
    tile_overrides[p_layer].erase(key);
    generated_tile_cache[p_layer].erase(key);
}

void WorldBubble::invalidate_region_cache(const Rect2i& p_rect, Layer p_layer) {
    auto erase_region = [&](std::unordered_map<uint64_t, uint16_t>& cache) {
        for (auto it = cache.begin(); it != cache.end(); ) {
            Vector3i pos = WorldCoords::unpack_coords_3d(it->first);
            if (pos.z == active_z && p_rect.has_point(Vector2i(pos.x, pos.y))) {
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
    current_light_samples.clear();
}

bool WorldBubble::set_entity(int x, int y, uint32_t entity_id) {
    uint64_t key = make_cell_key(x, y);
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
    uint64_t key = make_cell_key(x, y);
    entity_positions[key] = {entity_id};
}

void WorldBubble::remove_entity(int x, int y) {
    uint64_t key = make_cell_key(x, y);
    entity_positions.erase(key);
}

bool WorldBubble::update_entity_position(int old_x, int old_y, int new_x, int new_y, uint32_t entity_id) {
    uint64_t new_key = make_cell_key(new_x, new_y);
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

    uint64_t old_key = make_cell_key(old_x, old_y);
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
        if (e->z == active_z) {
            force_set_entity(e->x, e->y, id);
        }
    }
}

const WorldBubble::CellEntity* WorldBubble::get_entity_at(int x, int y) const {
    uint64_t key = make_cell_key(x, y);
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

Array WorldBubble::get_seen_cells_at_z(int z) const {
    Array a;
    for (uint64_t key : seen_cells) {
        Vector3i pos = WorldCoords::unpack_coords_3d(key);
        if (pos.z == z) {
            a.push_back(Vector2i(pos.x, pos.y));
        }
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
    uint64_t cell_key = make_cell_key(x, y);
    return seen_cells.count(cell_key) > 0;
}

std::vector<uint64_t> WorldBubble::consume_newly_seen_cells() {
    std::vector<uint64_t> out = newly_seen_cells;
    newly_seen_cells.clear();
    return out;
}

void WorldBubble::update_active_area(const CellArea& area) {
    std::vector<uint64_t> area_keys = area.world_keys();
    std::unordered_set<uint64_t> next_active;
    next_active.reserve(area_keys.size());

    newly_active_cells.clear();
    newly_active_cells.reserve(area_keys.size());
    for (uint64_t key : area_keys) {
        next_active.insert(key);
        if (active_cells.find(key) == active_cells.end()) {
            newly_active_cells.push_back(key);
        }
    }

    active_cells = std::move(next_active);
}

std::vector<uint64_t> WorldBubble::consume_newly_active_cells() {
    std::vector<uint64_t> out = newly_active_cells;
    newly_active_cells.clear();
    return out;
}

void WorldBubble::clear_active_area() {
    active_cells.clear();
    newly_active_cells.clear();
}

uint16_t WorldBubble::query_tile_id(int x, int y) {
    return query_tile_id_at_z(x, y, active_z);
}

uint16_t WorldBubble::query_tile_id_at_z(int x, int y, int z) {
    uint64_t cell_key = make_cell_key_at_z(x, y, z);
    return resolve_tile_id(LAYER_TILE, cell_key, x, y, z);
}

void WorldBubble::add_overlay(int x, int y, uint16_t atlas_x, uint16_t atlas_y, const Color& color, float lifetime) {
    uint64_t key = make_cell_key(x, y);
    Overlay ov;
    ov.atlas_x = atlas_x;
    ov.atlas_y = atlas_y;
    ov.color = color;
    ov.lifetime = lifetime;
    ov.age = 0.0f;
    overlays[key] = ov;
}

void WorldBubble::remove_overlay(int x, int y) {
    overlays.erase(make_cell_key(x, y));
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

std::vector<LightEmitter> WorldBubble::collect_light_emitters(
    const Vector2i& p_origin,
    const std::vector<uint64_t>& p_offset_keys,
    const EntityLedger* p_ledger
) {
    TileDb* tile_db = TileDb::get_singleton();
    ItemDb* item_db = ItemDb::get_singleton();
    RaceDb* race_db = RaceDb::get_singleton();

    std::vector<LightEmitter> emitters;
    std::unordered_set<uint64_t> offset_lookup;
    offset_lookup.reserve(p_offset_keys.size());
    for (uint64_t offset_key : p_offset_keys) {
        offset_lookup.insert(offset_key);
    }

    collect_tile_light_emitters(p_origin, p_offset_keys, tile_db, emitters);
    collect_dropped_item_light_emitters(p_origin, p_offset_keys, item_db, emitters);
    collect_entity_light_emitters(p_origin, offset_lookup, p_ledger, item_db, race_db, emitters);
    return emitters;
}

void WorldBubble::collect_tile_light_emitters(
    const Vector2i& p_origin,
    const std::vector<uint64_t>& p_offset_keys,
    TileDb* p_tile_db,
    std::vector<LightEmitter>& r_emitters
) {
    for (uint64_t offset_key : p_offset_keys) {
        const Vector2i offset = WorldCoords::unpack_coords(offset_key);
        const int world_x = p_origin.x + offset.x;
        const int world_y = p_origin.y + offset.y;
        const uint64_t cell_key = make_cell_key_at_z(world_x, world_y, active_z);
        const uint16_t tile_id = resolve_tile_id(LAYER_TILE, cell_key, world_x, world_y, active_z);
        const TileInfo* tile = p_tile_db ? p_tile_db->get_tile_info(tile_id) : nullptr;
        if (tile && tile->light.emits()) {
            append_light_emitter(r_emitters, Vector2i(world_x, world_y), active_z, tile->light.strength);
        }
    }
}

void WorldBubble::collect_dropped_item_light_emitters(
    const Vector2i& p_origin,
    const std::vector<uint64_t>& p_offset_keys,
    ItemDb* p_item_db,
    std::vector<LightEmitter>& r_emitters
) {
    for (uint64_t offset_key : p_offset_keys) {
        const Vector2i offset = WorldCoords::unpack_coords(offset_key);
        const int world_x = p_origin.x + offset.x;
        const int world_y = p_origin.y + offset.y;
        const uint64_t cell_key = make_cell_key_at_z(world_x, world_y, active_z);

        LightStrength strongest_ground_light = LIGHT_STRENGTH_BLANK;
        const std::vector<DroppedItem>* items = cell_data.get_items(cell_key);
        if (items) {
            for (const DroppedItem& item : *items) {
                if (item.amount <= 0) {
                    continue;
                }
                strongest_ground_light = light_strength_stronger(strongest_ground_light, item_light_strength_by_id(p_item_db, item.id));
            }
        }
        append_light_emitter(r_emitters, Vector2i(world_x, world_y), active_z, strongest_ground_light);
    }
}

void WorldBubble::collect_entity_light_emitters(
    const Vector2i& p_origin,
    const std::unordered_set<uint64_t>& p_offset_lookup,
    const EntityLedger* p_ledger,
    ItemDb* p_item_db,
    RaceDb* p_race_db,
    std::vector<LightEmitter>& r_emitters
) {
    if (!p_ledger || !entity_pool_source) {
        return;
    }

    for (const auto& entry : entity_positions) {
        const CellEntity& cell_entity = entry.second;
        const Entity* entity = entity_pool_source->get_entity(cell_entity.entity_id);
        if (!entity || entity->z != active_z) {
            continue;
        }

        const Vector2i entity_pos(entity->x, entity->y);
        const Vector2i entity_offset = entity_pos - p_origin;
        if (p_offset_lookup.find(WorldCoords::pack_coords(entity_offset.x, entity_offset.y)) == p_offset_lookup.end()) {
            continue;
        }

        const AnatomyData* anatomy = p_ledger->try_get_anatomy(entity->id);
        const RaceInfo* race = (anatomy && p_race_db) ? p_race_db->get_race_info(anatomy->race_id) : nullptr;
        if (race && race->light.emits()) {
            append_light_emitter(r_emitters, entity_pos, entity->z, race->light.strength);
        }

        const EquipmentData* equipment = p_ledger->try_get_equipment(entity->id);
        if (!equipment) {
            continue;
        }

        LightStrength strongest_equipment_light = LIGHT_STRENGTH_BLANK;
        for (const auto& slot_entry : equipment->slots) {
            const EquipmentSlot& slot = slot_entry.second;
            if (slot.item_id.is_empty()) {
                continue;
            }
            strongest_equipment_light = light_strength_stronger(
                strongest_equipment_light,
                item_light_strength_by_string(p_item_db, slot.item_id)
            );
        }
        append_light_emitter(r_emitters, entity_pos, entity->z, strongest_equipment_light);
    }
}

void WorldBubble::update_lighting(
    const Vector2i& p_origin,
    const std::vector<uint64_t>& p_offset_keys,
    bool p_lighting_enabled,
    const EntityLedger* p_ledger
) {
    current_light_samples.clear();
    current_light_samples.reserve(p_offset_keys.size());

    if (!p_lighting_enabled) {
        for (uint64_t offset_key : p_offset_keys) {
            Vector2i offset = WorldCoords::unpack_coords(offset_key);
            const int cx = p_origin.x + offset.x;
            const int cy = p_origin.y + offset.y;
            LightSample& sample = current_light_samples[make_cell_key_at_z(cx, cy, active_z)];
            sample.cell = LIGHT_STRENGTH_DAYLIGHT;
            sample.raise_all_faces(LIGHT_STRENGTH_DAYLIGHT);
        }
        return;
    }

    TileDb* tile_db = TileDb::get_singleton();
    std::vector<LightEmitter> emitters = collect_light_emitters(p_origin, p_offset_keys, p_ledger);

    LightMap::compute_natural_light(
        p_origin,
        active_z,
        p_offset_keys,
        [this](int world_x, int world_y, int world_z) {
            const uint64_t cell_key = make_cell_key_at_z(world_x, world_y, world_z);
            return resolve_tile_id(LAYER_TILE, cell_key, world_x, world_y, world_z);
        },
        [this, tile_db](int world_x, int world_y, int world_z) {
            const int above_z = world_z + 1;
            const uint64_t above_key = make_cell_key_at_z(world_x, world_y, above_z);
            const uint16_t above_tile_id = resolve_tile_id(LAYER_TILE, above_key, world_x, world_y, above_z);
            const TileInfo* above_info = tile_db ? tile_db->get_tile_info(above_tile_id) : nullptr;
            return tile_allows_sky(above_tile_id, above_info);
        },
        emitters,
        current_light_samples
    );

    static constexpr LightStrength PLAYER_MINIMUM_VISIBILITY = LIGHT_STRENGTH_LIT;
    for (int oy = -player_minimum_light_radius; oy <= player_minimum_light_radius; oy++) {
        for (int ox = -player_minimum_light_radius; ox <= player_minimum_light_radius; ox++) {
            if (!player_minimum_light_contains_offset(ox, oy, player_minimum_light_radius)) {
                continue;
            }

            const int world_x = p_origin.x + ox;
            const int world_y = p_origin.y + oy;
            const uint64_t cell_key = make_cell_key_at_z(world_x, world_y, active_z);
            LightSample& sample = current_light_samples[cell_key];
            const uint16_t tile_id = resolve_tile_id(LAYER_TILE, cell_key, world_x, world_y, active_z);
            const TileInfo* info = tile_db ? tile_db->get_tile_info(tile_id) : nullptr;
            if (tile_is_opaque(info)) {
                raise_view_facing_faces(sample, p_origin, Vector2i(world_x, world_y), PLAYER_MINIMUM_VISIBILITY);
            } else {
                sample.raise_cell(PLAYER_MINIMUM_VISIBILITY);
            }
        }
    }
}

LightLevel WorldBubble::get_current_light_level(uint64_t p_cell_key) const {
    return LightMap::get_level(current_light_samples, p_cell_key);
}

LightStrength WorldBubble::get_apparent_light_strength(const Vector2i& p_view_origin, int p_world_x, int p_world_y, int p_world_z) {
    const uint64_t cell_key = make_cell_key_at_z(p_world_x, p_world_y, p_world_z);
    const LightSample sample = LightMap::get_sample(current_light_samples, cell_key);

    TileDb* tile_db = TileDb::get_singleton();
    const uint16_t tile_id = resolve_tile_id(LAYER_TILE, cell_key, p_world_x, p_world_y, p_world_z);
    const TileInfo* info = tile_db ? tile_db->get_tile_info(tile_id) : nullptr;
    if (!tile_is_opaque(info)) {
        return sample.cell;
    }

    return view_facing_light(sample, p_view_origin, Vector2i(p_world_x, p_world_y));
}

LightLevel WorldBubble::get_apparent_light_level(const Vector2i& p_view_origin, int p_world_x, int p_world_y, int p_world_z) {
    return light_level_from_strength(get_apparent_light_strength(p_view_origin, p_world_x, p_world_y, p_world_z));
}

void WorldBubble::update_visibility(
    const Vector2i& player_pos,
    const std::vector<uint64_t>& offset_keys,
    bool occlusion_enabled
) {
    visible_cells.clear();
    if (current_light_samples.empty()) {
        update_lighting(player_pos, offset_keys, occlusion_enabled);
    }

    auto remember_visible_cell = [&](int x, int y, int z) {
        const uint64_t cell_key = make_cell_key_at_z(x, y, z);
        const LightLevel light_level = get_apparent_light_level(player_pos, x, y, z);
        if (!light_is_perceptible(light_level)) {
            return;
        }

        visible_cells.insert(cell_key);
        if (light_reveals_detail(light_level) && seen_cells.insert(cell_key).second) {
            newly_seen_cells.push_back(cell_key);
        }
    };

    auto propagate_vertical_air_visibility = [&]() {
        std::vector<Vector3i> active_cells;
        active_cells.reserve(visible_cells.size());
        for (uint64_t cell_key : visible_cells) {
            Vector3i pos = WorldCoords::unpack_coords_3d(cell_key);
            if (pos.z == active_z) {
                active_cells.push_back(pos);
            }
        }

        auto mark_vertical_cell = [&](int x, int y, int z) {
            const uint64_t cell_key = make_cell_key_at_z(x, y, z);
            visible_cells.insert(cell_key);
            const LightLevel light_level = get_apparent_light_level(player_pos, x, y, z);
            if (light_reveals_detail(light_level) && seen_cells.insert(cell_key).second) {
                newly_seen_cells.push_back(cell_key);
            }
        };

        for (const Vector3i& cell : active_cells) {
            const uint64_t cell_key = make_cell_key_at_z(cell.x, cell.y, cell.z);
            const uint16_t tile_id = resolve_tile_id(LAYER_TILE, cell_key, cell.x, cell.y, cell.z);
            const bool is_air = tile_is_air(tile_id);
            for (int direction = is_air ? -1 : 1; direction <= 1; direction += 2) {
                for (int depth = 1; depth <= VERTICAL_AIR_VISIBILITY_DEPTH; depth++) {
                    const int z = cell.z + direction * depth;
                    const uint64_t vertical_key = make_cell_key_at_z(cell.x, cell.y, z);
                    const uint16_t vertical_tile = resolve_tile_id(
                        LAYER_TILE, vertical_key, cell.x, cell.y, z
                    );
                    mark_vertical_cell(cell.x, cell.y, z);
                    if (!tile_is_air(vertical_tile)) {
                        break;
                    }
                }
            }
        }
    };

    if (!occlusion_enabled) {
        for (uint64_t offset_key : offset_keys) {
            Vector2i offset = WorldCoords::unpack_coords(offset_key);
            int cx = offset.x + player_pos.x;
            int cy = offset.y + player_pos.y;
            uint64_t cell_key = make_cell_key(cx, cy);
            resolve_tile_id(LAYER_TILE, cell_key, cx, cy, active_z);
            remember_visible_cell(cx, cy, active_z);
        }
        propagate_vertical_air_visibility();
        return;
    }

    std::unordered_map<uint64_t, uint16_t> visibility_tiles;
    visibility_tiles.reserve(offset_keys.size());
    for (uint64_t offset_key : offset_keys) {
        Vector2i offset = WorldCoords::unpack_coords(offset_key);
        int cx = offset.x + player_pos.x;
        int cy = offset.y + player_pos.y;
        uint64_t visibility_key = WorldCoords::pack_coords(cx, cy);
        uint64_t cell_key = make_cell_key(cx, cy);
        visibility_tiles[visibility_key] = resolve_tile_id(LAYER_TILE, cell_key, cx, cy, active_z);
    }

    std::unordered_set<uint64_t> visible_2d;
    Occlusion::compute_visible(player_pos, vision_radius, visibility_tiles, visible_2d);
    for (uint64_t visible_key : visible_2d) {
        Vector2i pos = WorldCoords::unpack_coords(visible_key);
        remember_visible_cell(pos.x, pos.y, active_z);
    }

    propagate_vertical_air_visibility();
}

WorldBubble::BubbleSnapshot WorldBubble::build_snapshot(
    const Vector2i& render_focus,
    const Vector2i& view_origin,
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
            int cx = ox + render_focus.x;
            int cy = oy + render_focus.y;
            uint64_t cell_key = make_cell_key(cx, cy);

            CellVisual visual;

            if (occlusion_enabled) {
                visual.occluded = visible_cells.find(cell_key) == visible_cells.end();
                visual.seen = seen_cells.count(cell_key) > 0;
                if (visual.occluded && !visual.seen) {
                    snapshot.cells[l][offset_key] = visual;
                    continue;
                }
                if (!visual.occluded) {
                    visual.light_level = get_apparent_light_level(view_origin, cx, cy, active_z);
                }
            } else {
                visual.light_level = LightLevel::Bright;
            }

            visual.tile_id = resolve_tile_id(l, cell_key, cx, cy, active_z);
            bool draw_dynamic = !occlusion_enabled || (!visual.occluded && light_reveals_dynamics(visual.light_level));
            bool can_draw_below_air = l == LAYER_TILE
                && tile_is_air(visual.tile_id)
                && (!occlusion_enabled || ((!visual.occluded && light_reveals_detail(visual.light_level)) || visual.seen));

            if (can_draw_below_air) {
                for (int depth = 1; depth <= VERTICAL_AIR_VISIBILITY_DEPTH; depth++) {
                    const int below_z = active_z - depth;
                    const uint64_t below_key = make_cell_key_at_z(cx, cy, below_z);
                    const uint16_t below_tile = resolve_tile_id(l, below_key, cx, cy, below_z);
                    if (below_tile != 0 && !tile_is_air(below_tile)) {
                        const bool below_known = !occlusion_enabled
                            || seen_cells.count(below_key) > 0
                            || visible_cells.count(below_key) > 0;
                        if (below_known) {
                            visual.draw_below_tile = true;
                            visual.below_tile_id = below_tile;
                            visual.below_depth = depth;
                        }
                        break;
                    }
                }
            }

            if (draw_dynamic && LAYER_HAS_ITEMS[l]) {
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
            if (draw_dynamic && ent_it != entity_positions.end()) {
                visual.entity_sprite_id = 1;
                if (entity_pool_source) {
                    const Entity* entity = entity_pool_source->get_entity(ent_it->second.entity_id);
                    if (entity) {
                        visual.entity_atlas_x = entity->atlas_x;
                        visual.entity_atlas_y = entity->atlas_y;
                    }
                }
            }

            if (draw_dynamic && l == LAYER_INDICATOR) {
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
