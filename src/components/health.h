#ifndef SPACETRAVELLER_HEALTH_H
#define SPACETRAVELLER_HEALTH_H

#include <cstdint>
#include <godot_cpp/variant/dictionary.hpp>

namespace godot {

struct HealthData {
    float current_blood = 100.0f;
    float max_blood = 100.0f;
    bool alive = true;
};

namespace Health {
    void init(HealthData& data, float max_blood);
    void drain_blood(HealthData& data, float amount);
    void kill(HealthData& data);
    bool is_alive(const HealthData& data);
    float get_blood_percent(const HealthData& data);
    Dictionary serialize(const HealthData& data);
    void deserialize(HealthData& data, const Dictionary& dict, float default_max_blood = 100.0f);
}

}

#endif // SPACETRAVELLER_HEALTH_H
