#ifndef SPACETRAVELLER_TURN_SCHEDULER_H
#define SPACETRAVELLER_TURN_SCHEDULER_H

#include <cstdint>
#include <vector>
#include <unordered_map>

namespace godot {

class TurnScheduler {
    struct Entry {
        float turn_time;
        uint32_t entity_id;
        uint64_t generation;
    };
    std::vector<Entry> heap;
    std::unordered_map<uint32_t, uint64_t> active_generation;
    uint64_t next_gen = 1;

    static bool compare(const Entry& a, const Entry& b) { return a.turn_time > b.turn_time; }
    void prune_stale_front();

public:
    TurnScheduler() = default;

    void push(uint32_t entity_id, float turn_time);
    uint32_t pop();
    void remove(uint32_t entity_id);
    void clear();
    float peek_time();
    int size() const { return static_cast<int>(active_generation.size()); }
    bool empty() const { return heap.empty(); }
};

}

#endif // SPACETRAVELLER_TURN_SCHEDULER_H
