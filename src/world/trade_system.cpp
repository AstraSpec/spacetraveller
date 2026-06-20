#include "trade_system.h"
#include "components/inventory.h"
#include "core/id_registry.h"
#include "core/rng.h"
#include "data/item_db.h"
#include "data/job_db.h"
#include "data/loot_db.h"

#include <limits>
#include <vector>

namespace godot {

namespace {

constexpr uint64_t VENDOR_COIN_STOCK_SALT = 0x56454E444F52434FULL; // "VENDORCO"

void add_stock_item(Dictionary& r_stock, IdRegistry* p_reg, const char* p_item_id, int p_amount) {
    if (!p_reg || p_amount <= 0) return;
    const uint16_t numeric_id = p_reg->get_id(p_item_id);
    if (numeric_id == 0) return;
    r_stock[p_reg->get_string(numeric_id)] = p_amount;
}

}

void TradeSystem::configure(EntityLedger* p_ledger, const int* p_world_seed) {
    ledger = p_ledger;
    world_seed = p_world_seed;
}

bool TradeSystem::begin_trade(uint32_t p_player_id, uint32_t p_vendor_id) {
    if (!ledger) return false;
    const EntityPool& pool = ledger->get_entity_pool();
    if (!pool.contains(p_player_id) || !pool.contains(p_vendor_id)) return false;
    if (!ledger->try_get_inventory(p_player_id)) return false;

    VendorState starting_vendor;
    const VendorState* saved_vendor = ledger->try_get_vendor_state(p_vendor_id);
    if (saved_vendor) {
        starting_vendor = *saved_vendor;
    } else if (!build_default_vendor_state(p_vendor_id, starting_vendor)) {
        return false;
    }

    session = TradeSession{};
    session.active = true;
    session.player_id = p_player_id;
    session.vendor_id = p_vendor_id;
    session.vendor_funds = starting_vendor.funds;
    session.credit = starting_vendor.credit;
    session.vendor_stock = starting_vendor.stock;
    return true;
}

void TradeSystem::end_trade() {
    session = TradeSession{};
}

bool TradeSystem::add_player_item(const String& p_item_id, int p_amount) {
    if (!session.active) return false;
    return add_player_offer_item(p_item_id, p_amount);
}

bool TradeSystem::add_vendor_item(const String& p_item_id, int p_amount) {
    if (!session.active) return false;
    if (!add_vendor_offer_item(p_item_id, p_amount)) return false;
    if (get_current_credit() < 0) {
        remove_offer_item(session.vendor_offer, p_item_id, p_amount);
        return false;
    }
    return true;
}

bool TradeSystem::remove_player_item(const String& p_item_id, int p_amount) {
    return remove_offer_item(session.player_offer, p_item_id, p_amount);
}

bool TradeSystem::remove_vendor_item(const String& p_item_id, int p_amount) {
    return remove_offer_item(session.vendor_offer, p_item_id, p_amount);
}

Dictionary TradeSystem::get_summary() const {
    Dictionary d;
    String reason;
    const bool valid = validate_session(&reason) && validate_offer_amounts(&reason);

    d["active"] = session.active;
    d["credit"] = get_current_credit();
    d["funds"] = get_current_funds();
    d["can_accept"] = valid && has_pending_offer() && is_trade_balanced();
    d["status"] = valid ? String("ok") : reason;
    d["player_offer"] = offer_to_array(session.player_offer, true);
    d["vendor_offer"] = offer_to_array(session.vendor_offer, false);
    d["vendor_stock"] = vendor_stock_to_dictionary();
    return d;
}

bool TradeSystem::can_accept_trade() const {
    String reason;
    if (!validate_session(&reason) || !validate_offer_amounts(&reason)) return false;
    return has_pending_offer() && is_trade_balanced();
}

bool TradeSystem::accept_trade() {
    if (!can_accept_trade() || !ledger) return false;

    InventoryData* player_inv = ledger->try_get_inventory(session.player_id);
    if (!player_inv) return false;

    InventoryData player_before = *player_inv;
    VendorState updated_vendor;
    updated_vendor.funds = session.vendor_funds;
    updated_vendor.credit = session.credit;
    updated_vendor.stock = session.vendor_stock;

    for (const auto& pair : session.player_offer) {
        if (!Inventory::remove_item(*player_inv, pair.first, pair.second)) {
            *player_inv = player_before;
            return false;
        }
    }
    for (const auto& pair : session.vendor_offer) {
        auto it = updated_vendor.stock.find(pair.first);
        if (it == updated_vendor.stock.end() || it->second < pair.second) {
            *player_inv = player_before;
            return false;
        }
        it->second -= pair.second;
        if (it->second == 0) updated_vendor.stock.erase(it);
    }
    for (const auto& pair : session.vendor_offer) {
        if (!Inventory::add_item(*player_inv, pair.first, pair.second)) {
            *player_inv = player_before;
            return false;
        }
    }
    for (const auto& pair : session.player_offer) {
        updated_vendor.stock[pair.first] += pair.second;
    }

    updated_vendor.funds = get_current_funds();
    updated_vendor.credit = get_current_credit();
    ledger->ensure_vendor_state(session.vendor_id) = updated_vendor;
    end_trade();
    return true;
}

bool TradeSystem::build_default_vendor_state(uint32_t p_vendor_id, VendorState& r_state) const {
    if (!ledger) return false;

    const SocialProfileData* profile = ledger->try_get_social_profile(p_vendor_id);
    JobDb* job_db = JobDb::get_singleton();
    const JobInfo* job_info = (profile && job_db) ? job_db->get_job_info(profile->job) : nullptr;
    if (!job_info || job_info->vendor_loot_table == 0) return false;

    const Entity* vendor_entity = ledger->get_entity_pool().get_entity(p_vendor_id);
    LootDb* loot_db = LootDb::get_singleton();
    if (!vendor_entity || !loot_db) return false;

    r_state = VendorState{};
    const uint32_t seed = world_seed ? static_cast<uint32_t>(*world_seed) : 0;
    const Vector2i vendor_pos(vendor_entity->x, vendor_entity->y);
    IdRegistry* reg = IdRegistry::get_singleton();
    if (reg) {
        Rng::Seeded coin_rng = Rng::at(seed, vendor_pos, Rng::VENDOR_LOOT, VENDOR_COIN_STOCK_SALT);
        const uint16_t bronze_id = reg->get_id("bronze_coin");
        const uint16_t silver_id = reg->get_id("silver_coin");
        if (bronze_id != 0) {
            r_state.stock[bronze_id] += coin_rng.range(1, 10);
        }
        const int silver_amount = coin_rng.range(0, 2);
        if (silver_id != 0 && silver_amount > 0) {
            r_state.stock[silver_id] += silver_amount;
        }
    }

    Rng::Seeded loot_rng = Rng::at(seed, vendor_pos, Rng::VENDOR_LOOT);
    std::vector<LootStack> stacks;
    if (!loot_db->roll_table(job_info->vendor_loot_table, loot_rng, stacks)) return false;
    for (const LootStack& stack : stacks) {
        if (stack.item_id == 0 || stack.amount <= 0) continue;
        r_state.stock[stack.item_id] += stack.amount;
    }
    return true;
}

int TradeSystem::get_inventory_amount(uint32_t p_entity_id, uint16_t p_item_id) const {
    if (!ledger) return 0;
    const InventoryData* inv = ledger->try_get_inventory(p_entity_id);
    return inv ? Inventory::get_item_amount(*inv, p_item_id) : 0;
}

int TradeSystem::get_vendor_stock_amount(uint16_t p_item_id) const {
    if (!session.active) return 0;
    auto it = session.vendor_stock.find(p_item_id);
    return it != session.vendor_stock.end() ? it->second : 0;
}

bool TradeSystem::add_player_offer_item(const String& p_item_id, int p_amount) {
    if (!session.active || p_amount <= 0) return false;
    IdRegistry* reg = IdRegistry::get_singleton();
    if (!reg) return false;

    const uint16_t item_id = reg->get_id(p_item_id);
    if (item_id == 0) return false;

    const int current_offered = session.player_offer.count(item_id) ? session.player_offer[item_id] : 0;
    if (current_offered + p_amount > get_inventory_amount(session.player_id, item_id)) return false;

    session.player_offer[item_id] = current_offered + p_amount;
    return true;
}

bool TradeSystem::add_vendor_offer_item(const String& p_item_id, int p_amount) {
    if (!session.active || p_amount <= 0) return false;
    IdRegistry* reg = IdRegistry::get_singleton();
    if (!reg) return false;

    const uint16_t item_id = reg->get_id(p_item_id);
    if (item_id == 0) return false;

    const int current_offered = session.vendor_offer.count(item_id) ? session.vendor_offer[item_id] : 0;
    if (current_offered + p_amount > get_vendor_stock_amount(item_id)) return false;

    session.vendor_offer[item_id] = current_offered + p_amount;
    return true;
}

bool TradeSystem::remove_offer_item(std::unordered_map<uint16_t, int>& p_offer, const String& p_item_id, int p_amount) {
    if (!session.active || p_amount <= 0) return false;
    IdRegistry* reg = IdRegistry::get_singleton();
    if (!reg) return false;

    const uint16_t item_id = reg->get_id(p_item_id);
    auto it = p_offer.find(item_id);
    if (it == p_offer.end()) return false;
    if (it->second < p_amount) return false;

    it->second -= p_amount;
    if (it->second == 0) {
        p_offer.erase(it);
    }
    return true;
}

bool TradeSystem::validate_session(String* r_reason) const {
    if (!session.active) {
        if (r_reason) *r_reason = "inactive";
        return false;
    }
    if (!ledger) {
        if (r_reason) *r_reason = "missing_ledger";
        return false;
    }
    const EntityPool& pool = ledger->get_entity_pool();
    if (!pool.contains(session.player_id)) {
        if (r_reason) *r_reason = "missing_player";
        return false;
    }
    if (!pool.contains(session.vendor_id)) {
        if (r_reason) *r_reason = "missing_vendor";
        return false;
    }
    if (!ledger->try_get_inventory(session.player_id)) {
        if (r_reason) *r_reason = "missing_inventory";
        return false;
    }
    return true;
}

bool TradeSystem::validate_offer_amounts(String* r_reason) const {
    if (!ledger) return false;
    for (const auto& pair : session.player_offer) {
        if (pair.second <= 0 || pair.second > get_inventory_amount(session.player_id, pair.first)) {
            if (r_reason) *r_reason = "invalid_player_offer";
            return false;
        }
    }
    for (const auto& pair : session.vendor_offer) {
        if (pair.second <= 0 || pair.second > get_vendor_stock_amount(pair.first)) {
            if (r_reason) *r_reason = "invalid_vendor_offer";
            return false;
        }
    }
    return true;
}

bool TradeSystem::has_pending_offer() const {
    return !session.player_offer.empty() || !session.vendor_offer.empty();
}

bool TradeSystem::is_trade_balanced() const {
    return get_current_credit() >= 0;
}

int TradeSystem::get_item_value(const String& p_item_id, int p_amount, bool p_selling_to_vendor) const {
    IdRegistry* reg = IdRegistry::get_singleton();
    if (!reg) return 0;

    const uint16_t item_id = reg->get_id(p_item_id);
    if (item_id == 0) return 0;

    return p_selling_to_vendor ? get_sell_value(item_id, p_amount) : get_buy_cost(item_id, p_amount);
}

int TradeSystem::get_percent_price(uint16_t p_item_id, int p_amount, int p_percent) const {
    ItemDb* item_db = ItemDb::get_singleton();
    const ItemInfo* info = item_db ? item_db->get_item_info(p_item_id) : nullptr;
    if (!info || p_amount <= 0 || info->price <= 0) return 0;

    int64_t value = (static_cast<int64_t>(info->price) * p_percent * p_amount) / 100;
    if (value > std::numeric_limits<int>::max()) {
        return std::numeric_limits<int>::max();
    }
    return value > 0 ? static_cast<int>(value) : 1;
}

int TradeSystem::get_sell_value(uint16_t p_item_id, int p_amount) const {
    return get_percent_price(p_item_id, p_amount, SELL_PERCENT);
}

int TradeSystem::get_buy_cost(uint16_t p_item_id, int p_amount) const {
    return get_percent_price(p_item_id, p_amount, BUY_PERCENT);
}

int TradeSystem::get_offer_total(const std::unordered_map<uint16_t, int>& p_offer, bool p_selling_to_vendor) const {
    int total = 0;
    for (const auto& pair : p_offer) {
        total += p_selling_to_vendor ? get_sell_value(pair.first, pair.second) : get_buy_cost(pair.first, pair.second);
    }
    return total;
}

int TradeSystem::get_sell_value_total() const {
    return get_offer_total(session.player_offer, true);
}

int TradeSystem::get_buy_cost_total() const {
    return get_offer_total(session.vendor_offer, false);
}

int TradeSystem::get_net_value() const {
    return get_sell_value_total() - get_buy_cost_total();
}

int TradeSystem::get_current_credit() const {
    return session.credit + get_net_value();
}

int TradeSystem::get_current_funds() const {
    return session.vendor_funds - get_net_value();
}

Array TradeSystem::offer_to_array(const std::unordered_map<uint16_t, int>& p_offer, bool p_selling_to_vendor) const {
    Array arr;
    IdRegistry* reg = IdRegistry::get_singleton();
    if (!reg) return arr;

    for (const auto& pair : p_offer) {
        Dictionary d;
        const int unit_value = p_selling_to_vendor ? get_sell_value(pair.first, 1) : get_buy_cost(pair.first, 1);
        d["id"] = reg->get_string(pair.first);
        d["amount"] = pair.second;
        d["unit_value"] = unit_value;
        d["value"] = p_selling_to_vendor ? get_sell_value(pair.first, pair.second) : get_buy_cost(pair.first, pair.second);
        arr.push_back(d);
    }
    return arr;
}

Dictionary TradeSystem::vendor_stock_to_dictionary() const {
    Dictionary stock;
    if (!session.active) return stock;
    IdRegistry* reg = IdRegistry::get_singleton();
    if (!reg) return stock;

    add_stock_item(stock, reg, "bronze_coin", get_vendor_stock_amount(reg->get_id("bronze_coin")));
    add_stock_item(stock, reg, "silver_coin", get_vendor_stock_amount(reg->get_id("silver_coin")));
    add_stock_item(stock, reg, "golden_coin", get_vendor_stock_amount(reg->get_id("golden_coin")));

    for (const auto& pair : session.vendor_stock) {
        if (pair.second > 0) {
            const String item_id = reg->get_string(pair.first);
            if (item_id == "bronze_coin" || item_id == "silver_coin" || item_id == "golden_coin") continue;
            stock[item_id] = pair.second;
        }
    }
    return stock;
}

}
