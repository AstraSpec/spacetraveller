#ifndef SPACETRAVELLER_PHYSIOLOGY_H
#define SPACETRAVELLER_PHYSIOLOGY_H

#include <godot_cpp/variant/dictionary.hpp>

namespace godot {

struct PhysiologyData {
    float current_consciousness = 100.0f;
    float max_consciousness = 100.0f;
    float pain = 0.0f;
    bool downed = false;
};

struct PhysiologyUpdate {
    float consciousness_lost = 0.0f;
    float pain_added = 0.0f;
    bool newly_downed = false;
    bool recovered = false;
    bool reached_zero = false;
};

namespace Physiology {
    void init(PhysiologyData& data, float max_consciousness = 100.0f);
    float get_consciousness_percent(const PhysiologyData& data);
    float get_consciousness_ceiling(
        const PhysiologyData& data,
        float blood_percent,
        float pain_floor = 0.0f);
    PhysiologyUpdate reconcile(
        PhysiologyData& data,
        float blood_percent,
        float pain_floor = 0.0f);
    PhysiologyUpdate apply_trauma(
        PhysiologyData& data,
        float consciousness_loss,
        float pain_gain,
        float blood_percent,
        float pain_floor = 0.0f);
    PhysiologyUpdate advance(
        PhysiologyData& data,
        float blood_percent,
        float elapsed,
        float action_recovery,
        float pain_floor = 0.0f);
    Dictionary serialize(const PhysiologyData& data);
    void deserialize(PhysiologyData& data, const Dictionary& dict);
}

}

#endif // SPACETRAVELLER_PHYSIOLOGY_H
