#ifndef SPACETRAVELLER_PATH_RESULT_H
#define SPACETRAVELLER_PATH_RESULT_H

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <vector>

namespace godot {

struct PathResult {
    bool found = false;
    std::vector<Vector2i> waypoints;
};

inline Array path_result_to_array(const PathResult& result) {
    Array arr;
    if (!result.found) {
        return arr;
    }
    for (const Vector2i& p : result.waypoints) {
        arr.push_back(Variant(p));
    }
    return arr;
}

}

#endif // SPACETRAVELLER_PATH_RESULT_H
