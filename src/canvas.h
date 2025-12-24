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

class Canvas {
private:
    int gridSize;
    std::vector<String> grid;

public:
    Canvas(int p_gridSize);
    
    void clear(const String &val = "");
    void setPixel(int x, int y, const String &val);
    String getPixel(int x, int y) const;
    
    void fillRect(int x, int y, int w, int h, const String &val);
    void drawLine(int x0, int y0, int x1, int y1, const String &val);
    void drawCircle(double xm, double ym, double r, const String &val);
    
    int get_grid_size() const { return gridSize; }
    
    void saveAsPPM(const std::string& filename) const;
};

}

#endif // SPACETRAVELLER_CANVAS_H
