#ifndef SPACETRAVELLER_TRADE_SYSTEM_H
#define SPACETRAVELLER_TRADE_SYSTEM_H

#include "entities/entity_ledger.h"
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <cstdint>
#include <unordered_map>

namespace godot {

struct TradeSession {
    bool active = false;
    uint32_t player_id = PLAYER_ENTITY_ID;
    uint32_t vendor_id = INVALID_ENTITY_ID;
    int vendor_funds = VendorState::DEFAULT_FUNDS;
    int credit = 0;
    std::unordered_map<uint16_t, int> player_offer;
    std::unordered_map<uint16_t, int> vendor_offer;
};

class TradeSystem {
public:
    static constexpr int SELL_PERCENT = 80;
    static constexpr int BUY_PERCENT = 120;

    void configure(EntityLedger* p_ledger);

    bool begin_trade(uint32_t p_player_id, uint32_t p_vendor_id);
    void end_trade();
    bool has_active_trade() const { return session.active; }

    bool add_player_item(const String& p_item_id, int p_amount);
    bool add_vendor_item(const String& p_item_id, int p_amount);
    bool remove_player_item(const String& p_item_id, int p_amount);
    bool remove_vendor_item(const String& p_item_id, int p_amount);

    Dictionary get_summary() const;
    bool can_accept_trade() const;
    bool accept_trade();
    int get_item_value(const String& p_item_id, int p_amount, bool p_selling_to_vendor) const;

private:
    EntityLedger* ledger = nullptr;
    TradeSession session;

    int get_inventory_amount(uint32_t p_entity_id, uint16_t p_item_id) const;
    bool add_offer_item(std::unordered_map<uint16_t, int>& p_offer, uint32_t p_owner_id, const String& p_item_id, int p_amount);
    bool remove_offer_item(std::unordered_map<uint16_t, int>& p_offer, const String& p_item_id, int p_amount);
    bool validate_session(String* r_reason = nullptr) const;
    bool validate_offer_amounts(String* r_reason = nullptr) const;
    bool has_pending_offer() const;
    bool is_trade_balanced() const;

    int get_percent_price(uint16_t p_item_id, int p_amount, int p_percent) const;
    int get_sell_value(uint16_t p_item_id, int p_amount) const;
    int get_buy_cost(uint16_t p_item_id, int p_amount) const;
    int get_offer_total(const std::unordered_map<uint16_t, int>& p_offer, bool p_selling_to_vendor) const;
    int get_sell_value_total() const;
    int get_buy_cost_total() const;
    int get_net_value() const;
    int get_current_credit() const;
    int get_current_funds() const;
    Array offer_to_array(const std::unordered_map<uint16_t, int>& p_offer, bool p_selling_to_vendor) const;
};

}

#endif // SPACETRAVELLER_TRADE_SYSTEM_H
