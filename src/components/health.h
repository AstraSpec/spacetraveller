#ifndef SPACETRAVELLER_HEALTH_H
#define SPACETRAVELLER_HEALTH_H

#include <cstdint>

namespace godot {

struct HealthData {
    float current_hp = 100.0f;
    float max_hp = 100.0f;
    bool alive = true;
};

namespace Health {
    void init(HealthData& data, float max_hp);
    void damage(HealthData& data, float amount);
    void heal(HealthData& data, float amount);
    inline bool is_alive(const HealthData& data);
    inline float get_percent(const HealthData& data);
}

}

#endif // SPACETRAVELLER_HEALTH_H
