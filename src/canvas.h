#ifndef SPACETRAVELLER_CANVAS_H
#define SPACETRAVELLER_CANVAS_H

#include <godot_cpp/variant/string.hpp>
#include <vector>
#include <string>
#include <cstdint>

namespace godot {

struct Point {
    int x, y;
};

// Standardized CityPixel metadata
// Bits 0-1: Orientation (0: South, 1: West, 2: North, 3: East)
// Bits 2-7: Reserved for variants or sub-types
struct CityPixel {
    uint16_t id = 0;
    uint8_t meta = 0;

    bool operator==(const CityPixel& p_other) const {
        return id == p_other.id && meta == p_other.meta;
    }

    bool operator!=(const CityPixel& p_other) const {
        return !(*this == p_other);
    }
};

class Canvas {
private:
    int gridSize;
    std::vector<CityPixel> grid;

public:
    Canvas(int p_gridSize);
    
    void clear(uint16_t p_id = 0, uint8_t p_meta = 0);
    void setPixel(int x, int y, uint16_t p_id, uint8_t p_meta = 0);
    CityPixel getPixel(int x, int y) const;
    
    void fillRect(int x, int y, int w, int h, uint16_t p_id, uint8_t p_meta = 0);
    void drawLine(int x0, int y0, int x1, int y1, uint16_t p_id, uint8_t p_meta = 0);
    void drawCircle(double xm, double ym, double r, uint16_t p_id, uint8_t p_meta = 0);
    
    int get_grid_size() const { return gridSize; }
};

}

#endif // SPACETRAVELLER_CANVAS_H
