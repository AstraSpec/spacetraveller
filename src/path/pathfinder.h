#ifndef SPACETRAVELLER_PATHFINDER_H
#define SPACETRAVELLER_PATHFINDER_H

#include "path_request.h"
#include "path_result.h"

namespace godot {

class TraversalSnapshot;

class Pathfinder {
public:
    virtual ~Pathfinder() = default;
    virtual PathResult find_path(const PathRequest& request, const TraversalSnapshot& traversal) const = 0;
};

}

#endif // SPACETRAVELLER_PATHFINDER_H
