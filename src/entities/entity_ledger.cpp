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

    float stam = 100.0f;
    if (race_db) {
        const RaceInfo* race = race_db->get_race_info(race_id);
        if (race) stam = race->base_stamina;
    }
    Stamina::init(stamina_data[id], stam);

    Equipment::init(equipment_data[id]);
    return id;
}

uint32_t EntityLedger::spawn_player(const Vector2i& pos, uint16_t atlas_x, uint16_t atlas_y) {
    uint32_t id = entity_pool.create_player_entity(pos.x, pos.y, atlas_x, atlas_y);
    
    Anatomy::init(anatomy_data[id], "human");
    Clothing::init(clothing_data[id]);
    Inventory::init(inventory_data[id]);
    Health::init(health_data[id], 100.0f);
    Stamina::init(stamina_data[id], 80.0f);
    Equipment::init(equipment_data[id]);
    
    return id;
}

void EntityLedger::destroy_entity(uint32_t id) {
    anatomy_data.erase(id);
    clothing_data.erase(id);
    inventory_data.erase(id);
    health_data.erase(id);
    stamina_data.erase(id);
    effects_data.erase(id);
    equipment_data.erase(id);
    locomotion_data.erase(id);
    perception_memory.erase(id);
    ai_data.erase(id);
    combat_style.erase(id);
    gender.erase(id);
    entity_name.erase(id);
    friendship.erase(id);
    romance.erase(id);
    
    entity_pool.destroy_entity(id);
}

int EntityLedger::get_inventory_item_amount(uint32_t id, const String& item_id) const {
    IdRegistry* reg = IdRegistry::get_singleton();
    if (!reg) return 0;

    auto it = inventory_data.find(id);
    if (it == inventory_data.end()) return 0;

    return Inventory::get_item_amount(it->second, reg->get_id(item_id));
}

void EntityLedger::init_relationship(uint32_t id) {
    if (friendship.find(id) != friendship.end() || romance.find(id) != romance.end()) {
        return; // already initialized; leave existing values unchanged
    }
    friendship[id] = RelationshipTuning::FRIENDSHIP_INITIAL;
    romance[id] = RelationshipTuning::ROMANCE_INITIAL;
}

bool EntityLedger::has_relationship(uint32_t id) const {
    return friendship.find(id) != friendship.end() || romance.find(id) != romance.end();
}

int EntityLedger::get_friendship(uint32_t id) const {
    auto it = friendship.find(id);
    if (it == friendship.end()) return RELATIONSHIP_SENTINEL;
    return it->second;
}

int EntityLedger::get_romance(uint32_t id) const {
    auto it = romance.find(id);
    if (it == romance.end()) return RELATIONSHIP_SENTINEL;
    return it->second;
}

void EntityLedger::set_friendship(uint32_t id, int value) {
    friendship[id] = CLAMP(value, RelationshipTuning::MIN_VALUE, RelationshipTuning::MAX_VALUE);
}

void EntityLedger::set_romance(uint32_t id, int value) {
    romance[id] = CLAMP(value, RelationshipTuning::MIN_VALUE, RelationshipTuning::MAX_VALUE);
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

Dictionary EntityLedger::get_health(uint32_t id) const {
    Dictionary d;
    auto it = health_data.find(id);
    if (it == health_data.end()) return d;
    d["current_hp"] = it->second.current_hp;
    d["max_hp"] = it->second.max_hp;
    d["alive"] = it->second.alive;
    return d;
}

Dictionary EntityLedger::get_stamina(uint32_t id) const {
    auto it = stamina_data.find(id);
    if (it == stamina_data.end()) return Dictionary();
    return Stamina::serialize(it->second);
}

Dictionary EntityLedger::get_effects(uint32_t id) const {
    auto it = effects_data.find(id);
    if (it == effects_data.end()) return Dictionary();
    return Effects::serialize(it->second);
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
    Array entities;
    for (const auto& e : entity_pool.get_all()) {
        entities.push_back(serialize_entity(e.id));
    }
    data["entities"] = entities;
    return data;
}

void EntityLedger::deserialize(const Dictionary& data) {
    entity_pool.clear();
    anatomy_data.clear();
    clothing_data.clear();
    inventory_data.clear();
    health_data.clear();
    stamina_data.clear();
    effects_data.clear();
    equipment_data.clear();
    locomotion_data.clear();
    perception_memory.clear();
    ai_data.clear();
    combat_style.clear();
    gender.clear();
    entity_name.clear();
    friendship.clear();
    romance.clear();

    Array entities = data.get("entities", Array());
    for (int i = 0; i < entities.size(); i++) {
        deserialize_entity(entities[i]);
    }
}

Dictionary EntityLedger::serialize_entity(uint32_t id) const {
    Dictionary data;
    const Entity* e = entity_pool.get_entity(id);
    if (!e) return data;

    data["id"] = static_cast<int64_t>(id);
    data["x"] = e->x;
    data["y"] = e->y;
    data["atlas_x"] = static_cast<int>(e->atlas_x);
    data["atlas_y"] = static_cast<int>(e->atlas_y);
    data["next_turn_time"] = e->next_turn_time;

    auto anat_it = anatomy_data.find(id);
    if (anat_it != anatomy_data.end()) {
        data["anatomy"] = Anatomy::serialize(anat_it->second);
    }

    auto cloth_it = clothing_data.find(id);
    if (cloth_it != clothing_data.end()) {
        data["clothing"] = Clothing::serialize(cloth_it->second);
    }

    auto inv_it = inventory_data.find(id);
    if (inv_it != inventory_data.end()) {
        data["inventory"] = Inventory::serialize(inv_it->second);
    }

    auto hp_it = health_data.find(id);
    if (hp_it != health_data.end()) {
        data["health"] = Health::serialize(hp_it->second);
    }

    auto stam_it = stamina_data.find(id);
    if (stam_it != stamina_data.end()) {
        data["stamina"] = Stamina::serialize(stam_it->second);
    }

    auto fx_it = effects_data.find(id);
    if (fx_it != effects_data.end() && !fx_it->second.effects.empty()) {
        data["effects"] = Effects::serialize(fx_it->second);
    }

    auto eq_it = equipment_data.find(id);
    if (eq_it != equipment_data.end()) {
        data["equipment"] = Equipment::serialize(eq_it->second);
    }

    auto loco_it = locomotion_data.find(id);
    if (loco_it != locomotion_data.end()) {
        data["locomotion"] = Locomotion::serialize(loco_it->second);
    }

    auto mem_it = perception_memory.find(id);
    if (mem_it != perception_memory.end()) {
        data["perception"] = Perception::serialize(mem_it->second);
    }

    auto ai_it = ai_data.find(id);
    if (ai_it != ai_data.end()) {
        data["ai"] = AIController::serialize(ai_it->second);
    }

    auto style_it = combat_style.find(id);
    if (style_it != combat_style.end()) {
        data["combat_style"] = style_it->second;
    }

    auto gender_it = gender.find(id);
    if (gender_it != gender.end()) {
        data["gender"] = gender_it->second;
    }

    auto name_it = entity_name.find(id);
    if (name_it != entity_name.end()) {
        data["name"] = name_it->second;
    }

    auto fr_it = friendship.find(id);
    if (fr_it != friendship.end()) {
        data["friendship"] = fr_it->second;
    }

    auto ro_it = romance.find(id);
    if (ro_it != romance.end()) {
        data["romance"] = ro_it->second;
    }

    return data;
}

uint32_t EntityLedger::deserialize_entity(const Dictionary& data) {
    uint32_t id = static_cast<uint32_t>(static_cast<int64_t>(data.get("id", static_cast<int64_t>(0))));
    int x = data.get("x", 0);
    int y = data.get("y", 0);
    uint16_t atlas_x = static_cast<uint16_t>(static_cast<int>(data.get("atlas_x", 0)));
    uint16_t atlas_y = static_cast<uint16_t>(static_cast<int>(data.get("atlas_y", 0)));

    entity_pool.create_entity_with_id(id, x, y, atlas_x, atlas_y);

    Entity* e = entity_pool.get_entity(id);
    if (e) {
        e->next_turn_time = static_cast<float>(static_cast<double>(data.get("next_turn_time", 0.0)));
    }

    if (data.has("anatomy")) {
        AnatomyData ad;
        Anatomy::deserialize(ad, data["anatomy"]);
        anatomy_data[id] = ad;
    }

    if (data.has("clothing")) {
        ClothingData cd;
        Clothing::deserialize(cd, data["clothing"]);
        clothing_data[id] = cd;
    }

    if (data.has("inventory")) {
        InventoryData idata;
        Inventory::deserialize(idata, data["inventory"]);
        inventory_data[id] = idata;
    }

    if (data.has("health")) {
        Health::deserialize(health_data[id], data["health"]);
    }

    if (data.has("stamina")) {
        Stamina::deserialize(stamina_data[id], data["stamina"]);
    }

    if (data.has("effects")) {
        Effects::deserialize(effects_data[id], data["effects"]);
    }

    if (data.has("equipment")) {
        EquipmentData ed;
        Equipment::deserialize(ed, data["equipment"]);
        equipment_data[id] = ed;
    }

    if (data.has("locomotion")) {
        Locomotion::deserialize(locomotion_data[id], data["locomotion"]);
    }

    if (data.has("perception")) {
        Perception::deserialize(perception_memory[id], data["perception"]);
    }

    if (data.has("ai")) {
        AIController::deserialize(ai_data[id], data["ai"]);
    }

    if (data.has("combat_style")) {
        combat_style[id] = data["combat_style"];
    }

    if (data.has("gender")) {
        gender[id] = data["gender"];
    }

    if (data.has("name")) {
        entity_name[id] = data["name"];
    }

    if (data.has("friendship") || data.has("romance")) {
        int f = static_cast<int>(data.get("friendship", RelationshipTuning::FRIENDSHIP_INITIAL));
        int r = static_cast<int>(data.get("romance", RelationshipTuning::ROMANCE_INITIAL));
        friendship[id] = CLAMP(f, RelationshipTuning::MIN_VALUE, RelationshipTuning::MAX_VALUE);
        romance[id]    = CLAMP(r, RelationshipTuning::MIN_VALUE, RelationshipTuning::MAX_VALUE);
    }

    return id;
}