#include "trade_system.h"
#include "components/inventory.h"
#include "core/id_registry.h"
#include "data/item_db.h"

#include <limits>

namespace godot {

void TradeSystem::configure(EntityLedger* p_ledger) {
    ledger = p_ledger;
}

bool TradeSystem::begin_trade(uint32_t p_player_id, uint32_t p_vendor_id) {
    if (!ledger) return false;
    const EntityPool& pool = ledger->get_entity_pool();
    if (!pool.contains(p_player_id) || !pool.contains(p_vendor_id)) return false;
    if (!ledger->try_get_inventory(p_player_id) || !ledger->try_get_inventory(p_vendor_id)) return false;

    session = TradeSession{};
    session.active = true;
    session.player_id = p_player_id;
    session.vendor_id = p_vendor_id;
    const VendorState* saved_vendor = ledger->try_get_vendor_state(p_vendor_id);
    session.vendor_funds = saved_vendor ? saved_vendor->funds : VendorState::DEFAULT_FUNDS;
    session.credit = saved_vendor ? saved_vendor->credit : 0;
    return true;
}

void TradeSystem::end_trade() {
    session = TradeSession{};
}

bool TradeSystem::add_player_item(const String& p_item_id, int p_amount) {
    if (!session.active) return false;
    return add_offer_item(session.player_offer, session.player_id, p_item_id, p_amount);
}

bool TradeSystem::add_vendor_item(const String& p_item_id, int p_amount) {
    if (!session.active) return false;
    if (!add_offer_item(session.vendor_offer, session.vendor_id, p_item_id, p_amount)) return false;
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
    InventoryData* vendor_inv = ledger->try_get_inventory(session.vendor_id);
    if (!player_inv || !vendor_inv) return false;

    InventoryData player_before = *player_inv;
    InventoryData vendor_before = *vendor_inv;

    for (const auto& pair : session.player_offer) {
        if (!Inventory::remove_item(*player_inv, pair.first, pair.second)) {
            *player_inv = player_before;
            *vendor_inv = vendor_before;
            return false;
        }
    }
    for (const auto& pair : session.vendor_offer) {
        if (!Inventory::remove_item(*vendor_inv, pair.first, pair.second)) {
            *player_inv = player_before;
            *vendor_inv = vendor_before;
            return false;
        }
    }
    for (const auto& pair : session.vendor_offer) {
        if (!Inventory::add_item(*player_inv, pair.first, pair.second)) {
            *player_inv = player_before;
            *vendor_inv = vendor_before;
            return false;
        }
    }
    for (const auto& pair : session.player_offer) {
        if (!Inventory::add_item(*vendor_inv, pair.first, pair.second)) {
            *player_inv = player_before;
            *vendor_inv = vendor_before;
            return false;
        }
    }

    VendorState& vendor_state = ledger->ensure_vendor_state(session.vendor_id);
    vendor_state.funds = get_current_funds();
    vendor_state.credit = get_current_credit();
    end_trade();
    return true;
}

int TradeSystem::get_inventory_amount(uint32_t p_entity_id, uint16_t p_item_id) const {
    if (!ledger) return 0;
    const InventoryData* inv = ledger->try_get_inventory(p_entity_id);
    return inv ? Inventory::get_item_amount(*inv, p_item_id) : 0;
}

bool TradeSystem::add_offer_item(std::unordered_map<uint16_t, int>& p_offer, uint32_t p_owner_id, const String& p_item_id, int p_amount) {
    if (!session.active || p_amount <= 0) return false;
    IdRegistry* reg = IdRegistry::get_singleton();
    if (!reg) return false;

    const uint16_t item_id = reg->get_id(p_item_id);
    if (item_id == 0) return false;

    const int current_offered = p_offer.count(item_id) ? p_offer[item_id] : 0;
    if (current_offered + p_amount > get_inventory_amount(p_owner_id, item_id)) return false;

    p_offer[item_id] = current_offered + p_amount;
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
    if (!ledger->try_get_inventory(session.player_id) || !ledger->try_get_inventory(session.vendor_id)) {
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
        if (pair.second <= 0 || pair.second > get_inventory_amount(session.vendor_id, pair.first)) {
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

}
