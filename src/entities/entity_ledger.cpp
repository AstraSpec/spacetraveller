#include "entity_ledger.h"
#include "core/id_registry.h"
#include "data/item_db.h"
#include "data/race_db.h"

using namespace godot;

uint32_t EntityLedger::spawn_entity(const Vector2i& pos, uint16_t atlas_x, uint16_t atlas_y, const String& race_id) {
    return spawn_entity(pos, 0, atlas_x, atlas_y, race_id);
}

uint32_t EntityLedger::spawn_entity(const Vector2i& pos, int z, uint16_t atlas_x, uint16_t atlas_y, const String& race_id) {
    uint32_t id = entity_pool.create_entity(pos.x, pos.y, z, atlas_x, atlas_y);
    
    Anatomy::init(anatomy_data[id], race_id);
    Clothing::init(clothing_data[id]);
    Inventory::init(inventory_data[id]);

    float hp = 100.0f;
    float stam = 100.0f;
    RaceDb* race_db = RaceDb::get_singleton();
    if (race_db) {
        const RaceInfo* race = race_db->get_race_info(race_id);
        if (race) {
            hp = race->base_hp;
            stam = race->base_stamina;
        }
    }
    Health::init(health_data[id], hp);
    Stamina::init(stamina_data[id], stam);

    Equipment::init(equipment_data[id]);
    SocialMemory::init(social_data[id]);
    return id;
}

uint32_t EntityLedger::spawn_player(const Vector2i& pos, uint16_t atlas_x, uint16_t atlas_y) {
    uint32_t id = entity_pool.create_player_entity(pos.x, pos.y, atlas_x, atlas_y);
    
    Anatomy::init(anatomy_data[id], "human");
    Clothing::init(clothing_data[id]);
    Inventory::init(inventory_data[id]);
    Health::init(health_data[id], 10000.0f);
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
    social_data.erase(id);
    social_profiles.erase(id);
    vendor_state.erase(id);
    
    entity_pool.destroy_entity(id);
}

InventoryData& EntityLedger::ensure_inventory(uint32_t id) {
    auto it = inventory_data.find(id);
    if (it != inventory_data.end()) return it->second;
    InventoryData& inv = inventory_data[id];
    Inventory::init(inv);
    return inv;
}

EffectsData& EntityLedger::ensure_effects(uint32_t id) {
    return effects_data[id];
}

SocialMemoryData& EntityLedger::ensure_social_memory(uint32_t id) {
    auto it = social_data.find(id);
    if (it != social_data.end()) return it->second;
    SocialMemoryData& memory = social_data[id];
    SocialMemory::init(memory);
    return memory;
}

VendorState& EntityLedger::ensure_vendor_state(uint32_t id) {
    return vendor_state[id];
}

bool EntityLedger::has_inventory(uint32_t id) const {
    return inventory_data.find(id) != inventory_data.end();
}

bool EntityLedger::is_alive(uint32_t id) const {
    const HealthData* hp = try_get_health(id);
    return hp && hp->alive;
}

bool EntityLedger::is_player(uint32_t id) const {
    return id == EntityPool::PLAYER_ID && entity_pool.contains(id);
}

bool EntityLedger::is_actor(uint32_t id) const {
    return entity_pool.contains(id) && try_get_locomotion(id) != nullptr;
}

bool EntityLedger::is_npc(uint32_t id) const {
    return entity_pool.contains(id) && id != EntityPool::PLAYER_ID && try_get_ai(id) != nullptr;
}

bool EntityLedger::is_combatant(uint32_t id) const {
    return entity_pool.contains(id)
        && try_get_anatomy(id) != nullptr
        && try_get_health(id) != nullptr
        && try_get_stamina(id) != nullptr
        && try_get_equipment(id) != nullptr;
}

bool EntityLedger::is_sapient(uint32_t id) const {
    const AnatomyData* anatomy = try_get_anatomy(id);
    if (!anatomy) return false;
    RaceDb* race_db = RaceDb::get_singleton();
    return race_db && race_db->has_tag(anatomy->race_id, "SAPIENT");
}

bool EntityLedger::has_core_components(uint32_t id) const {
    return entity_pool.contains(id)
        && anatomy_data.find(id) != anatomy_data.end()
        && inventory_data.find(id) != inventory_data.end()
        && health_data.find(id) != health_data.end()
        && stamina_data.find(id) != stamina_data.end()
        && equipment_data.find(id) != equipment_data.end();
}

bool EntityLedger::is_schedulable_actor(uint32_t id) const {
    if (id == EntityPool::PLAYER_ID) return validate_player(id);
    return validate_npc_actor(id);
}

bool EntityLedger::validate_player(uint32_t id) const {
    return id == EntityPool::PLAYER_ID
        && has_core_components(id)
        && try_get_locomotion(id) != nullptr;
}

bool EntityLedger::validate_npc_actor(uint32_t id) const {
    return id != EntityPool::PLAYER_ID
        && has_core_components(id)
        && try_get_locomotion(id) != nullptr
        && try_get_ai(id) != nullptr
        && try_get_perception(id) != nullptr;
}

bool EntityLedger::validate_combatant(uint32_t id) const {
    return is_combatant(id) && is_alive(id);
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

void EntityLedger::init_social_profile(uint32_t id, const String& job, const String& dialogue_profile) {
    SocialProfileData& profile = social_profiles[id];
    SocialProfile::init(profile);
    profile.job = job.is_empty() ? String("drifter") : job;
    profile.dialogue_profile = dialogue_profile.is_empty() ? String("default") : dialogue_profile;

    auto anat_it = anatomy_data.find(id);
    if (anat_it != anatomy_data.end()) {
        profile.faction = anat_it->second.race_id;
    }
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

Dictionary EntityLedger::get_equipment(uint32_t id) const {
    auto it = equipment_data.find(id);
    if (it == equipment_data.end()) return Dictionary();
    return Equipment::serialize(it->second);
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

Dictionary EntityLedger::get_social_profile(uint32_t id) const {
    auto it = social_profiles.find(id);
    if (it == social_profiles.end()) return Dictionary();
    return SocialProfile::serialize(it->second);
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

static bool is_weapon_slot(const String& slot_name) {
    return slot_name == Equipment::MAIN_HAND_SLOT || slot_name == Equipment::OFF_HAND_SLOT;
}

static int get_item_grasp_required(const String& item_id, ItemDb* db) {
    if (!db) return 1;
    Dictionary weapon = db->get_weapon_data(item_id);
    if (weapon.is_empty()) return 1;
    return MAX(1, static_cast<int>(weapon.get("grasp_required", 1)));
}

bool EntityLedger::wield_weapon(uint32_t id, const String& slot_name, const String& item_id) {
    if (!is_weapon_slot(slot_name)) return false;
    if (get_inventory_item_amount(id, item_id) <= 0) return false;

    ItemDb* db = ItemDb::get_singleton();
    if (!db) return false;
    if (!db->get_item_info(item_id)) return false;

    auto anat_it = anatomy_data.find(id);
    auto equip_it = equipment_data.find(id);
    if (anat_it == anatomy_data.end() || equip_it == equipment_data.end()) return false;
    if (Equipment::is_slot_occupied(equip_it->second, slot_name)) return false;

    int total_required = get_item_grasp_required(item_id, db);
    for (const auto& pair : equip_it->second.slots) {
        if (!is_weapon_slot(pair.first)) continue;
        total_required += get_item_grasp_required(pair.second.item_id, db);
    }

    int functional_grasps = Anatomy::count_functional_parts_with_tag(anat_it->second, "GRASP");
    if (total_required > functional_grasps) return false;

    return Equipment::equip(equip_it->second, slot_name, item_id);
}

bool EntityLedger::unwield_weapon(uint32_t id, const String& slot_name) {
    if (!is_weapon_slot(slot_name)) return false;
    auto equip_it = equipment_data.find(id);
    if (equip_it == equipment_data.end()) return false;
    return Equipment::unequip(equip_it->second, slot_name);
}

bool EntityLedger::wield_weapon_by_string(uint32_t id, const String& item_id) {
    auto equip_it = equipment_data.find(id);
    if (equip_it == equipment_data.end()) return false;

    if (!Equipment::is_slot_occupied(equip_it->second, Equipment::MAIN_HAND_SLOT)) {
        return wield_weapon(id, Equipment::MAIN_HAND_SLOT, item_id);
    }
    if (!Equipment::is_slot_occupied(equip_it->second, Equipment::OFF_HAND_SLOT)) {
        return wield_weapon(id, Equipment::OFF_HAND_SLOT, item_id);
    }
    return false;
}

Dictionary EntityLedger::serialize() const {
    Dictionary data;
    Array entities;
    for (uint32_t id : entity_pool.get_live_ids()) {
        entities.push_back(serialize_entity(id));
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
    social_data.clear();
    social_profiles.clear();
    vendor_state.clear();

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
    data["z"] = e->z;
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

    auto soc_it = social_data.find(id);
    if (soc_it != social_data.end()) {
        data["social"] = SocialMemory::serialize(soc_it->second);
    }

    auto profile_it = social_profiles.find(id);
    if (profile_it != social_profiles.end()) {
        data["social_profile"] = SocialProfile::serialize(profile_it->second);
    }

    auto vendor_it = vendor_state.find(id);
    if (vendor_it != vendor_state.end()) {
        Dictionary vendor;
        vendor["funds"] = vendor_it->second.funds;
        vendor["credit"] = vendor_it->second.credit;
        data["vendor"] = vendor;
    }

    return data;
}

uint32_t EntityLedger::deserialize_entity(const Dictionary& data) {
    if (!data.has("id")) return EntityPool::INVALID_ID;

    uint32_t id = static_cast<uint32_t>(static_cast<int64_t>(data.get("id", static_cast<int64_t>(0))));
    int x = data.get("x", 0);
    int y = data.get("y", 0);
    int z = data.get("z", 0);
    uint16_t atlas_x = static_cast<uint16_t>(static_cast<int>(data.get("atlas_x", 0)));
    uint16_t atlas_y = static_cast<uint16_t>(static_cast<int>(data.get("atlas_y", 0)));

    entity_pool.create_entity_with_id(id, x, y, z, atlas_x, atlas_y);

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

    if (data.has("social")) {
        SocialMemoryData sd;
        SocialMemory::deserialize(sd, data["social"]);
        social_data[id] = sd;
    }

    if (data.has("social_profile")) {
        SocialProfileData sp;
        SocialProfile::deserialize(sp, data["social_profile"]);
        social_profiles[id] = sp;
    }

    if (data.has("vendor")) {
        Dictionary vendor = data["vendor"];
        VendorState state;
        state.funds = static_cast<int>(vendor.get("funds", VendorState::DEFAULT_FUNDS));
        if (state.funds < 0) state.funds = 0;
        state.credit = static_cast<int>(vendor.get("credit", 0));
        if (state.credit < 0) state.credit = 0;
        vendor_state[id] = state;
    }

    return id;
}

int EntityLedger::get_social_cooldown(uint32_t id) const {
    auto it = social_data.find(id);
    if (it == social_data.end()) return 0;
    return it->second.cooldown_available_at;
}

void EntityLedger::set_social_cooldown(uint32_t id, int turn) {
    social_data[id].cooldown_available_at = turn;
}

String EntityLedger::get_social_state_json(uint32_t id) const {
    auto it = social_data.find(id);
    if (it == social_data.end()) return "";
    return it->second.conversation_state_json;
}

void EntityLedger::set_social_state_json(uint32_t id, const String& json) {
    social_data[id].conversation_state_json = json;
}

void EntityLedger::clear_social_state(uint32_t id) {
    auto it = social_data.find(id);
    if (it == social_data.end()) return;
    it->second.conversation_state_json = "";
}
