#include "entity_ledger.h"
#include "core/id_registry.h"
#include "data/item_db.h"
#include "data/race_db.h"

using namespace godot;

uint32_t EntityLedger::spawn_entity(const Vector2i& pos, uint16_t atlas_x, uint16_t atlas_y, const String& race_id) {
    uint32_t id = entity_pool.create_entity(pos.x, pos.y, atlas_x, atlas_y);
    
    Anatomy::init(anatomy_data[id], race_id);
    Clothing::init(clothing_data[id]);
    Inventory::init(inventory_data[id]);

    float hp = 100.0f;
    RaceDb* race_db = RaceDb::get_singleton();
    if (race_db) {
        const RaceInfo* race = race_db->get_race_info(race_id);
        if (race) hp = race->base_hp;
    }
    Health::init(health_data[id], hp);

    Equipment::init(equipment_data[id]);
    return id;
}

uint32_t EntityLedger::spawn_player(const Vector2i& pos, uint16_t atlas_x, uint16_t atlas_y) {
    uint32_t id = entity_pool.create_player_entity(pos.x, pos.y, atlas_x, atlas_y);
    
    Anatomy::init(anatomy_data[id], "human");
    Clothing::init(clothing_data[id]);
    Inventory::init(inventory_data[id]);
    Health::init(health_data[id], 100.0f);
    Equipment::init(equipment_data[id]);
    
    return id;
}

void EntityLedger::destroy_entity(uint32_t id) {
    anatomy_data.erase(id);
    clothing_data.erase(id);
    inventory_data.erase(id);
    health_data.erase(id);
    equipment_data.erase(id);
    locomotion_data.erase(id);
    perception_memory.erase(id);
    ai_data.erase(id);
    
    entity_pool.destroy_entity(id);
}

void EntityLedger::init_inventory(uint32_t id) {
    Inventory::init(inventory_data[id]);
}

void EntityLedger::init_anatomy(uint32_t id, const String& race_id) {
    Anatomy::init(anatomy_data[id], race_id);
    Clothing::init(clothing_data[id]);
}

int EntityLedger::get_inventory_item_amount(uint32_t id, const String& item_id) const {
    IdRegistry* reg = IdRegistry::get_singleton();
    if (!reg) return 0;

    auto it = inventory_data.find(id);
    if (it == inventory_data.end()) return 0;

    return Inventory::get_item_amount(it->second, reg->get_id(item_id));
}

Dictionary EntityLedger::get_anatomy(uint32_t id) const {
    auto it = anatomy_data.find(id);
    if (it == anatomy_data.end()) return Dictionary();
    return Anatomy::serialize(it->second);
}

Dictionary EntityLedger::get_clothing(uint32_t id) const {
    auto it = clothing_data.find(id);
    if (it == clothing_data.end()) return Dictionary();
    return Clothing::serialize(it->second);
}

Dictionary EntityLedger::get_inventory(uint32_t id) const {
    auto it = inventory_data.find(id);
    if (it == inventory_data.end()) return Dictionary();
    return Inventory::serialize(it->second);
}

float EntityLedger::get_inventory_weight(uint32_t id) const {
    auto it = inventory_data.find(id);
    if (it == inventory_data.end()) return 0.0f;
    return Inventory::get_total_weight(it->second);
}

float EntityLedger::get_inventory_volume(uint32_t id) const {
    auto it = inventory_data.find(id);
    if (it == inventory_data.end()) return 0.0f;
    return Inventory::get_total_volume(it->second);
}

float EntityLedger::get_armor_rating(uint32_t id) const {
    auto anat_it = anatomy_data.find(id);
    auto cloth_it = clothing_data.find(id);
    if (anat_it == anatomy_data.end() || cloth_it == clothing_data.end()) return 0.0f;
    return Clothing::get_armor(cloth_it->second, anat_it->second);
}

String EntityLedger::get_anatomy_part_name(uint32_t id, int part_index) const {
    auto it = anatomy_data.find(id);
    if (it == anatomy_data.end()) return "Unknown";
    return Anatomy::get_name(it->second, part_index);
}

bool EntityLedger::add_inventory_item(uint32_t id, const String& item_id, int amount) {
    IdRegistry* reg = IdRegistry::get_singleton();
    if (!reg) return false;
    
    auto it = inventory_data.find(id);
    if (it == inventory_data.end()) return false;
    
    return Inventory::add_item(it->second, reg->get_id(item_id), amount);
}

bool EntityLedger::remove_inventory_item(uint32_t id, const String& item_id, int amount) {
    IdRegistry* reg = IdRegistry::get_singleton();
    if (!reg) return false;
    
    auto it = inventory_data.find(id);
    if (it == inventory_data.end()) return false;
    
    return Inventory::remove_item(it->second, reg->get_id(item_id), amount);
}

bool EntityLedger::equip_clothing(uint32_t id, int part_index, const String& item_id, const String& layer) {
    auto anat_it = anatomy_data.find(id);
    auto cloth_it = clothing_data.find(id);
    if (anat_it == anatomy_data.end() || cloth_it == clothing_data.end()) return false;
    
    return Clothing::equip(cloth_it->second, anat_it->second, part_index, item_id, layer);
}

bool EntityLedger::unequip_clothing(uint32_t id, const String& item_id) {
    auto it = clothing_data.find(id);
    if (it == clothing_data.end()) return false;
    return Clothing::unequip(it->second, item_id);
}

bool EntityLedger::equip_clothing_by_string(uint32_t id, const String& item_id) {
    ItemDb* db = ItemDb::get_singleton();
    if (!db) return false;
    
    Dictionary clothing_info = db->get_clothing_data(item_id);
    if (clothing_info.is_empty()) return false;
    
    String part_type = clothing_info.get("part", "");
    String layer = clothing_info.get("layer", "middle");
    
    auto anat_it = anatomy_data.find(id);
    auto cloth_it = clothing_data.find(id);
    if (anat_it == anatomy_data.end() || cloth_it == clothing_data.end()) return false;
    
    for (int i = 0; i < anat_it->second.parts.size(); i++) {
        if (anat_it->second.parts[i].type_id == part_type && Anatomy::is_functional(anat_it->second, i)) {
            return Clothing::equip(cloth_it->second, anat_it->second, i, item_id, layer);
        }
    }
    return false;
}

bool EntityLedger::unequip_clothing_by_string(uint32_t id, const String& item_id) {
    return unequip_clothing(id, item_id);
}

Dictionary EntityLedger::serialize() const {
    Dictionary data;
    
    data["pool"] = entity_pool.serialize();
    
    Dictionary anat_data;
    for (const auto& [id, ad] : anatomy_data) {
        anat_data[static_cast<int64_t>(id)] = Anatomy::serialize(ad);
    }
    data["anatomy"] = anat_data;
    
    Dictionary cloth_data;
    for (const auto& [id, cd] : clothing_data) {
        cloth_data[static_cast<int64_t>(id)] = Clothing::serialize(cd);
    }
    data["clothing"] = cloth_data;
    
    Dictionary inv_data;
    for (const auto& [id, idata] : inventory_data) {
        inv_data[static_cast<int64_t>(id)] = Inventory::serialize(idata);
    }
    data["inventory"] = inv_data;
    
    Dictionary hp_data;
    for (const auto& [id, hd] : health_data) {
        Dictionary d;
        d["current_hp"] = hd.current_hp;
        d["max_hp"] = hd.max_hp;
        d["alive"] = hd.alive;
        hp_data[static_cast<int64_t>(id)] = d;
    }
    data["health"] = hp_data;
    
    Dictionary eq_data;
    for (const auto& [id, ed] : equipment_data) {
        eq_data[static_cast<int64_t>(id)] = Equipment::serialize(ed);
    }
    data["equipment"] = eq_data;
    
    Dictionary loco_data;
    for (const auto& [id, ld] : locomotion_data) {
        Dictionary d;
        d["speed"] = ld.speed;
        Array path_arr;
        for (const auto& p : ld.path) path_arr.push_back(Vector2i(p.x, p.y));
        d["path"] = path_arr;
        d["path_index"] = ld.path_index;
        loco_data[static_cast<int64_t>(id)] = d;
    }
    data["locomotion"] = loco_data;
    
    Dictionary mem_data;
    for (const auto& [id, pm] : perception_memory) {
        Dictionary d;
        Array known_tiles;
        for (uint64_t k : pm.known_tiles) known_tiles.push_back(static_cast<int64_t>(k));
        d["known_tiles"] = known_tiles;
        Array known_entities;
        for (uint64_t e : pm.known_entities) known_entities.push_back(static_cast<int64_t>(e));
        d["known_entities"] = known_entities;
        d["last_known_player_x"] = pm.last_known_player_pos.x;
        d["last_known_player_y"] = pm.last_known_player_pos.y;
        d["player_seen"] = pm.player_seen;
        mem_data[static_cast<int64_t>(id)] = d;
    }
    data["perception"] = mem_data;
    
    Dictionary ai_save;
    for (const auto& [id, ad] : ai_data) {
        Dictionary d;
        d["state"] = static_cast<int>(ad.state);
        d["perception_tier"] = static_cast<int>(ad.perception_tier);
        d["wander_center_x"] = ad.wander_center.x;
        d["wander_center_y"] = ad.wander_center.y;
        d["wander_radius"] = ad.wander_radius;
        d["wander_cooldown"] = ad.wander_cooldown;
        d["stuck_counter"] = ad.stuck_counter;
        ai_save[static_cast<int64_t>(id)] = d;
    }
    data["ai"] = ai_save;
    
    return data;
}

void EntityLedger::deserialize(const Dictionary& data) {
    entity_pool.deserialize(data.get("pool", Dictionary()));
    
    anatomy_data.clear();
    Dictionary anat_data = data.get("anatomy", Dictionary());
    Array anat_ids = anat_data.keys();
    for (int i = 0; i < anat_ids.size(); i++) {
        AnatomyData ad;
        Anatomy::deserialize(ad, anat_data[anat_ids[i]]);
        anatomy_data[static_cast<uint32_t>(static_cast<int64_t>(anat_ids[i]))] = ad;
    }
    
    clothing_data.clear();
    Dictionary cloth_data = data.get("clothing", Dictionary());
    Array cloth_ids = cloth_data.keys();
    for (int i = 0; i < cloth_ids.size(); i++) {
        ClothingData cd;
        Clothing::deserialize(cd, cloth_data[cloth_ids[i]]);
        clothing_data[static_cast<uint32_t>(static_cast<int64_t>(cloth_ids[i]))] = cd;
    }
    
    inventory_data.clear();
    Dictionary inv_data = data.get("inventory", Dictionary());
    Array inv_ids = inv_data.keys();
    for (int i = 0; i < inv_ids.size(); i++) {
        InventoryData idata;
        Inventory::deserialize(idata, inv_data[inv_ids[i]]);
        inventory_data[static_cast<uint32_t>(static_cast<int64_t>(inv_ids[i]))] = idata;
    }
    
    health_data.clear();
    Dictionary hp_data = data.get("health", Dictionary());
    Array hp_ids = hp_data.keys();
    for (int i = 0; i < hp_ids.size(); i++) {
        Dictionary d = hp_data[hp_ids[i]];
        HealthData hd;
        hd.current_hp = static_cast<float>(static_cast<double>(d.get("current_hp", 100.0)));
        hd.max_hp = static_cast<float>(static_cast<double>(d.get("max_hp", 100.0)));
        hd.alive = d.get("alive", true);
        health_data[static_cast<uint32_t>(static_cast<int64_t>(hp_ids[i]))] = hd;
    }
    
    equipment_data.clear();
    Dictionary eq_data = data.get("equipment", Dictionary());
    Array eq_ids = eq_data.keys();
    for (int i = 0; i < eq_ids.size(); i++) {
        EquipmentData ed;
        Equipment::deserialize(ed, eq_data[eq_ids[i]]);
        equipment_data[static_cast<uint32_t>(static_cast<int64_t>(eq_ids[i]))] = ed;
    }
    
    locomotion_data.clear();
    Dictionary loco_data = data.get("locomotion", Dictionary());
    Array loco_ids = loco_data.keys();
    for (int i = 0; i < loco_ids.size(); i++) {
        Dictionary d = loco_data[loco_ids[i]];
        LocomotionData ld;
        ld.speed = static_cast<float>(static_cast<double>(d.get("speed", 1.0)));
        Array path_arr = d.get("path", Array());
        for (int j = 0; j < path_arr.size(); j++) {
            ld.path.push_back(path_arr[j]);
        }
        ld.path_index = static_cast<int>(d.get("path_index", 0));
        locomotion_data[static_cast<uint32_t>(static_cast<int64_t>(loco_ids[i]))] = ld;
    }
    
    perception_memory.clear();
    Dictionary mem_data = data.get("perception", Dictionary());
    Array mem_ids = mem_data.keys();
    for (int i = 0; i < mem_ids.size(); i++) {
        Dictionary d = mem_data[mem_ids[i]];
        PerceptionMemory pm;
        Array known_tiles = d.get("known_tiles", Array());
        for (int j = 0; j < known_tiles.size(); j++) {
            pm.known_tiles.insert(static_cast<uint64_t>(static_cast<int64_t>(known_tiles[j])));
        }
        Array known_entities = d.get("known_entities", Array());
        for (int j = 0; j < known_entities.size(); j++) {
            pm.known_entities.insert(static_cast<uint64_t>(static_cast<int64_t>(known_entities[j])));
        }
        pm.last_known_player_pos = Vector2i(
            static_cast<int>(d.get("last_known_player_x", 0)),
            static_cast<int>(d.get("last_known_player_y", 0))
        );
        pm.player_seen = d.get("player_seen", false);
        perception_memory[static_cast<uint32_t>(static_cast<int64_t>(mem_ids[i]))] = pm;
    }
    
    ai_data.clear();
    Dictionary ai_save = data.get("ai", Dictionary());
    Array ai_ids = ai_save.keys();
    for (int i = 0; i < ai_ids.size(); i++) {
        Dictionary d = ai_save[ai_ids[i]];
        AIData ad;
        ad.state = static_cast<AIState>(static_cast<int>(d.get("state", 0)));
        ad.perception_tier = static_cast<PerceptionTier>(static_cast<int>(d.get("perception_tier", 0)));
        ad.wander_center = Vector2i(
            static_cast<int>(d.get("wander_center_x", 0)),
            static_cast<int>(d.get("wander_center_y", 0))
        );
        ad.wander_radius = static_cast<float>(static_cast<double>(d.get("wander_radius", 10.0)));
        ad.wander_cooldown = static_cast<int>(d.get("wander_cooldown", 0));
        ad.stuck_counter = static_cast<int>(d.get("stuck_counter", 0));
        ai_data[static_cast<uint32_t>(static_cast<int64_t>(ai_ids[i]))] = ad;
    }
}