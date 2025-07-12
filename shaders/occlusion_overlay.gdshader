shader_type canvas_item;

uniform sampler2D tile_info;       // grayscale visibility: 1 = visible, 0.5 = wall
uniform vec2 map_size;             // in tiles
uniform vec2 tile_size;            // in pixels
uniform vec2 player_tile_pos;      // in tile coordinates (relative to texture)

void fragment() {
    // Convert fragment UV to tile coordinate in tile_info texture
    vec2 screen_pos_px = UV * map_size * tile_size; // position in pixels
    vec2 screen_tile_pos = screen_pos_px / tile_size; // tile coords
    
    ivec2 tile_coord = ivec2(floor(screen_tile_pos));
    
    // If out of bounds, fully occluded
    bool out_of_bounds = (
        tile_coord.x < 0 || tile_coord.y < 0 ||
        tile_coord.x >= int(map_size.x) || tile_coord.y >= int(map_size.y)
    );
    
    bool blocked = false;
    
    if (!out_of_bounds) {
        // Bresenham's line algorithm from player_tile_pos to tile_coord
        
        ivec2 from = ivec2(floor(player_tile_pos));
        ivec2 to = tile_coord;
        ivec2 delta = abs(to - from);
        ivec2 step = ivec2(
            (to.x > from.x) ? 1 : -1,
            (to.y > from.y) ? 1 : -1
        );
        
        int err = delta.x - delta.y;
        ivec2 current = from;
        
        while (true) {
            // Skip the starting tile (player position) so it doesn't occlude itself
            if (current != from && current != to) {
    // only check tiles *between* player and target tile for blocking
    float tile_value = texelFetch(tile_info, current, 0).r;
    if (tile_value < 0.75) {
        blocked = true;
        break;
    }
}

            
            if (current == to) {
                break;
            }
            
            int e2 = 2 * err;
            if (e2 > -delta.y) {
                err -= delta.y;
                current.x += step.x;
            }
            if (e2 < delta.x) {
                err += delta.x;
                current.y += step.y;
            }
        }
    } else {
        blocked = true;
    }
    
    if (blocked) {
        COLOR = vec4(0.1, 0.1, 0.1, 0.75); // darken occluded
    } else {
        discard;
    }
}
