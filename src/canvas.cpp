#include "canvas.h"
#include <cmath>
#include <algorithm>
#include <fstream>

namespace godot {

Canvas::Canvas(int p_gridSize) : gridSize(p_gridSize) {
    grid.assign(gridSize * gridSize, "");
}

void Canvas::clear(const String &val) {
    std::fill(grid.begin(), grid.end(), val);
}

void Canvas::setPixel(int x, int y, const String &val) {
    if (x >= 0 && x < gridSize && y >= 0 && y < gridSize) {
        grid[y * gridSize + x] = val;
    }
}

String Canvas::getPixel(int x, int y) const {
    if (x >= 0 && x < gridSize && y >= 0 && y < gridSize) {
        return grid[y * gridSize + x];
    }
    return "void";
}

void Canvas::fillRect(int x, int y, int w, int h, const String &val) {
    for (int iy = y; iy < y + h; ++iy) {
        for (int ix = x; ix < x + w; ++ix) {
            setPixel(ix, iy, val);
        }
    }
}

void Canvas::drawLine(int x0, int y0, int x1, int y1, const String &val) {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    while (true) {
        setPixel(x0, y0, val);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;

        // If we're about to move diagonally, we add a cardinal step to ensure 4-connectivity.
        if (e2 >= dy && e2 <= dx) {
            if (dx > -dy) {
                setPixel(x0 + sx, y0, val);
            } else {
                setPixel(x0, y0 + sy, val);
            }
        }

        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void Canvas::drawCircle(double xm, double ym, double r, const String &val) {
    if (r <= 0) return;
    int x = std::round(r), y = 0, err = 1 - x;

    while (x >= y) {
        // Draw the 8 symmetric points
        for (int sx : {-1, 1}) for (int sy : {-1, 1}) {
            setPixel(std::round(xm + sx * x), std::round(ym + sy * y), val);
            setPixel(std::round(xm + sx * y), std::round(ym + sy * x), val);
        }

        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            // Bridge the gap for 4-connectivity before moving x
            for (int sx : {-1, 1}) for (int sy : {-1, 1}) {
                setPixel(std::round(xm + sx * x), std::round(ym + sy * y), val);
                setPixel(std::round(xm + sx * y), std::round(ym + sy * x), val);
            }
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void Canvas::saveAsPPM(const std::string& filename) const {
    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs.is_open()) return;
    
    ofs << "P6\n" << gridSize << " " << gridSize << "\n255\n";
    for (int y = 0; y < gridSize; ++y) {
        for (int x = 0; x < gridSize; ++x) {
            unsigned char r, g, b;
            const String& chunkId = grid[y * gridSize + x];
            
            if (chunkId == "road") { r=107; g=107; b=107; }
            else if (chunkId == "alley") { r=68;  g=68;  b=68;  }
            else if (chunkId == "building") { r=163; g=78;  b=28;  }
            else if (chunkId == "palace") { r=245; g=158; b=11;  }
            else if (chunkId == "water") { r=30;  g=58;  b=138; }
            else if (chunkId == "gate") { r=148; g=163; b=184; }
            else if (chunkId == "plaza") { r=87;  g=83;  b=78;  }
            else if (chunkId == "forest") { r=34; g=139; b=34; }
            else if (chunkId == "plains") { r=144; g=238; b=144; }
            else { r=38;  g=38;  b=38; } // default/empty
            
            ofs.put(r); ofs.put(g); ofs.put(b);
        }
    }
    ofs.close();
}

}
