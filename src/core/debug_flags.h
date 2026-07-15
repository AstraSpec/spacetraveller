#ifndef SPACETRAVELLER_DEBUG_FLAGS_H
#define SPACETRAVELLER_DEBUG_FLAGS_H

namespace godot::DebugFlags {

// Runtime-only debug state. It intentionally is not part of save data.
inline bool debug_mode_enabled = false;

inline void set_debug_mode_enabled(bool p_enabled) {
    debug_mode_enabled = p_enabled;
}

inline bool is_debug_mode_enabled() {
    return debug_mode_enabled;
}

}

#endif // SPACETRAVELLER_DEBUG_FLAGS_H
