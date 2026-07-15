#ifndef SPACETRAVELLER_PERCEPTION_H
#define SPACETRAVELLER_PERCEPTION_H

#include <godot_cpp/variant/vector2i.hpp>

namespace godot {

class WorldBubble;
class TileDb;

namespace Perception {
    bool has_line_of_sight(int x1, int y1, int x2, int y2,
                           const WorldBubble& bubble, const TileDb& tile_db);
}

}

#endif // SPACETRAVELLER_PERCEPTION_H
