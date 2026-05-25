#ifndef SPACETRAVELLER_A_STAR_GRID_H
#define SPACETRAVELLER_A_STAR_GRID_H

#include "pathfinder.h"

namespace godot {

class AStarGridPathfinder : public Pathfinder {
public:
    PathResult find_path(const PathRequest& request, const TraversalSnapshot& traversal) const override;
};

}

#endif // SPACETRAVELLER_A_STAR_GRID_H
