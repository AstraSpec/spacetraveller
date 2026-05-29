#include "turn_scheduler.h"
#include "entities/entity.h"
#include <algorithm>
#include <cmath>

using namespace godot;

void TurnScheduler::push(uint32_t entity_id, float turn_time) {
    uint64_t gen = next_gen++;
    active_generation[entity_id] = gen;
    heap.push_back({turn_time, entity_id, gen});
    std::push_heap(heap.begin(), heap.end(), compare);
}

uint32_t TurnScheduler::pop() {
    while (!heap.empty()) {
        Entry e = heap.front();
        std::pop_heap(heap.begin(), heap.end(), compare);
        heap.pop_back();

        auto it = active_generation.find(e.entity_id);
        if (it != active_generation.end() && it->second == e.generation) {
            return e.entity_id;
        }
    }
    return INVALID_ENTITY_ID;
}

void TurnScheduler::remove(uint32_t entity_id) {
    active_generation.erase(entity_id);
}

void TurnScheduler::clear() {
    heap.clear();
    active_generation.clear();
}

float TurnScheduler::peek_time() const {
    if (heap.empty()) return INFINITY;
    return heap.front().turn_time;
}
