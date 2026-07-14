#ifndef SPACETRAVELLER_QUEST_TRACKER_H
#define SPACETRAVELLER_QUEST_TRACKER_H

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <string>
#include <unordered_map>
#include "sim/game_event.h"
#include "core/string_hasher.h"

namespace godot {

class EntityLedger;
class QuestDb;
class WorldGenerator;
namespace Rng { struct Seeded; }

struct QuestInstance {
    String id;
    String template_kind;  // "gather" | "*_gather" | "kill" | "reach"
    String status;         // "offered" | "active" | "completed" | "declined" | "failed"
    uint32_t giver_entity_id = 0;
    int     target = 0;
    int     progress = 0;
    int     started_turn = -1;
    float   deadline_turn = -1.0f;
    Dictionary params;     // {"item_id":"flint","race_id":"bear","target_job":"bandit","tile_id":"oak",…}
    Dictionary rewards;    // baked at offer time
};

struct QuestFailureHistory {
    String template_kind;
    uint32_t giver_entity_id = 0;
    bool ever_failed = false;
    float last_failed_turn = -1.0f;
    int failure_count = 0;
};

class QuestTracker : public IGameEventListener {
    EntityLedger* ledger = nullptr;
    QuestDb*      db     = nullptr;
    WorldGenerator* generator = nullptr;
    uint32_t      player_entity_id = 0;
    const int*    world_seed = nullptr;

    using EmitFn = void(*)(void* userdata, const String& quest_id);
    EmitFn   emit_quest_updated = nullptr;
    void*    emit_userdata      = nullptr;

    std::unordered_map<String, QuestInstance, StringHasher> instances;
    std::unordered_map<String, QuestFailureHistory, StringHasher> failure_history;

public:
    QuestTracker() = default;

    void configure(
        EntityLedger* p_ledger,
        QuestDb* p_db,
        uint32_t p_player_id,
        const int* p_world_seed = nullptr,
        WorldGenerator* p_generator = nullptr
    );

    void set_emit_callback(EmitFn p_emit, void* p_userdata);

    void on_game_event(const GameEvent& p_event) override;

    Array generate_offers(uint32_t p_giver_entity_id, int p_count = 1);
    Dictionary generate_offer(uint32_t p_giver_entity_id, const String& p_kind);
    Dictionary generate_story_offer(uint32_t p_giver_entity_id, const String& p_kind);
    bool  accept(const String& p_quest_id);
    bool  decline(const String& p_quest_id);
    bool  fail_for_dead_giver(uint32_t p_giver_entity_id);
    bool  can_complete(const String& p_quest_id);
    bool  complete(const String& p_quest_id);

    Array      get_offers_for(uint32_t p_giver_entity_id);
    Array      get_active();
    Array      get_completed();
    Array      get_offered();
    Dictionary get_quest(const String& p_quest_id);
    bool       is_completed(const String& p_quest_id);
    bool       has_failed(uint32_t p_giver_entity_id, const String& p_quest_ref);
    bool       can_offer(uint32_t p_giver_entity_id, const String& p_kind);

    Dictionary serialize() const;
    void       deserialize(const Dictionary& p_data);

private:
    QuestInstance _sample_one(const String& p_kind, uint32_t p_giver_entity_id);
    Rng::Seeded   _quest_rng(const String& p_kind, uint32_t p_giver_entity_id) const;
    bool          _is_gather_kind(const String& p_kind) const;
    bool          _location_matches(const String& p_kind, const Vector2i& p_position, int p_z) const;
    bool          _giver_can_offer(const String& p_kind, uint32_t p_giver_entity_id) const;
    float         _current_turn() const;
    String        _failure_key(uint32_t p_giver_entity_id, const String& p_kind) const;
    const QuestFailureHistory* _get_failure_history(uint32_t p_giver_entity_id, const String& p_kind) const;
    bool          _failure_allows_offer(uint32_t p_giver_entity_id, const String& p_kind) const;
    void          _record_failure(const QuestInstance& p_q);
    void          _fail(const String& p_quest_id, const String& p_reason);
    void          _expire_due_quests();
    void          _advance(const String& p_quest_id, int p_delta);
    bool          _has_required_items(const QuestInstance& p_q) const;
    bool          _remove_required_items(const QuestInstance& p_q);
    void          _mark_completed(const String& p_quest_id);
    void          _apply_rewards(const QuestInstance& p_q);
    void          _emit(const String& p_quest_id);

    Dictionary _view(const String& p_quest_id, const QuestInstance& p_q) const;
};

}

#endif // SPACETRAVELLER_QUEST_TRACKER_H
