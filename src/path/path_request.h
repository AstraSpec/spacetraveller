#ifndef SPACETRAVELLER_PATH_REQUEST_H
#define SPACETRAVELLER_PATH_REQUEST_H

#include <godot_cpp/variant/vector2i.hpp>
#include <cstdint>

namespace godot {

enum PathRequestFlags : uint32_t {
    PATH_FLAG_ALLOW_DIAGONAL = 1 << 0,
};

struct PathRequest {
    Vector2i start;
    Vector2i goal;
    int goal_radius = 0;
    uint32_t flags = 0;

    bool allow_diagonal() const {
        return (flags & PATH_FLAG_ALLOW_DIAGONAL) != 0;
    }
};

}

#endif // SPACETRAVELLER_PATH_REQUEST_H
