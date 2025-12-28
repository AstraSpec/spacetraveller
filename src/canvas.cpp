#include "canvas.h"
#include "data/id_registry.h"
#include <cmath>
#include <algorithm>
#include <fstream>

namespace godot {

Canvas::Canvas(int p_gridSize) : gridSize(p_gridSize) {
    grid.assign(gridSize * gridSize, {0, 0});
}

void Canvas::clear(uint16_t p_id, uint8_t p_meta) {
    std::fill(grid.begin(), grid.end(), CityPixel{p_id, p_meta});
}

void Canvas::setPixel(int x, int y, uint16_t p_id, uint8_t p_meta) {
    if (x >= 0 && x < gridSize && y >= 0 && y < gridSize) {
        grid[y * gridSize + x] = {p_id, p_meta};
    }
}

CityPixel Canvas::getPixel(int x, int y) const {
    if (x >= 0 && x < gridSize && y >= 0 && y < gridSize) {
        return grid[y * gridSize + x];
    }
    return {0, 0};
}

void Canvas::fillRect(int x, int y, int w, int h, uint16_t p_id, uint8_t p_meta) {
    for (int iy = y; iy < y + h; ++iy) {
        for (int ix = x; ix < x + w; ++ix) {
            setPixel(ix, iy, p_id, p_meta);
        }
    }
}

void Canvas::drawLine(int x0, int y0, int x1, int y1, uint16_t p_id, uint8_t p_meta) {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    while (true) {
        setPixel(x0, y0, p_id, p_meta);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;

        // If we're about to move diagonally, we add a cardinal step to ensure 4-connectivity.
        if (e2 >= dy && e2 <= dx) {
            if (dx > -dy) {
                setPixel(x0 + sx, y0, p_id, p_meta);
            } else {
                setPixel(x0, y0 + sy, p_id, p_meta);
            }
        }

        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void Canvas::drawCircle(double xm, double ym, double r, uint16_t p_id, uint8_t p_meta) {
    if (r <= 0) return;
    int x = std::round(r), y = 0, err = 1 - x;

    while (x >= y) {
        // Draw the 8 symmetric points
        for (int sx : {-1, 1}) for (int sy : {-1, 1}) {
            setPixel(std::round(xm + sx * x), std::round(ym + sy * y), p_id, p_meta);
            setPixel(std::round(xm + sx * y), std::round(ym + sy * x), p_id, p_meta);
        }

        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            for (int sx : {-1, 1}) for (int sy : {-1, 1}) {
                setPixel(std::round(xm + sx * x), std::round(ym + sy * y), p_id, p_meta);
                setPixel(std::round(xm + sx * y), std::round(ym + sy * x), p_id, p_meta);
            }
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

}
