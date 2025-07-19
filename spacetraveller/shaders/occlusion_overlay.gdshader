shader_type canvas_item;

uniform sampler2D tile_info;       // red: 1.0 = visible, 0.25 = occluded, 0.0 = never seen
uniform vec2 map_size;             // in tiles
uniform vec2 tile_size;            // in pixels

void fragment() {
    vec2 screen_pos_px = UV * map_size * tile_size;
    vec2 screen_tile_pos = screen_pos_px / tile_size;
    ivec2 tile_coord = ivec2(floor(screen_tile_pos));

    // Default to full black
    COLOR = vec4(0.0, 0.0, 0.0, 1.0);

    // Only process if inside bounds
    if (
        tile_coord.x >= 0 && tile_coord.y >= 0 &&
        tile_coord.x < int(map_size.x) && tile_coord.y < int(map_size.y)
    ) {
        float value = texelFetch(tile_info, tile_coord, 0).r;

        if (value == 0.0) {
            COLOR = vec4(0.0, 0.0, 0.0, 1.0); // Never seen
        } else if (value < 0.75) {
            COLOR = vec4(0.1, 0.1, 0.1, 0.75); // Occluded
        } else {
            discard; // Fully visible
        }
    }
}
