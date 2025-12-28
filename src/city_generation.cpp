#include "city_generation.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

CityGeneration::CityGeneration(Canvas& p_canvas, uint32_t seed) : canvas(p_canvas), rng(seed) {
    randomize();
}

double CityGeneration::randomDouble() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng);
}

void CityGeneration::randomize() {
    spokeJitters.clear();
    for (int i = 0; i < 32; ++i) spokeJitters.push_back((randomDouble() - 0.5) * 2.0);

    spawnRands.clear();
    for (int i = 0; i < 12; ++i) spawnRands.push_back(randomDouble());
}

bool CityGeneration::isInSector(int px, int py, double cx, double cy, double a1, double a2, double r1, double r2) {
    double dx = px - cx, dy = py - cy;
    double d = std::hypot(dx, dy);
    if (d < r1 - 0.5 || d > r2 + 0.5) return false;
    double angle = std::atan2(dy, dx);
    if (angle < 0) angle += Math_PI * 2.0;

    auto normalize = [](double a) {
        double res = fmod(a, Math_PI * 2.0);
        if (res < 0) res += Math_PI * 2.0;
        return res;
    };

    double s = normalize(a1);
    double e = normalize(a2);

    return s > e ? (angle >= s || angle <= e) : (angle >= s && angle <= e);
}

bool CityGeneration::canPlacePixel(int x, int y, const String &val) {
    String current = canvas.getPixel(x, y);
    if (val == "road") {
        return (current != "water" && current != "palace" && current != "gate");
    } else {
        return (!(current == "void" && (val == "alley" || val == "building")) &&
                current != "water" && current != "palace" && current != "gate" &&
                !(current == "road" && (val == "alley" || val == "building")) &&
                !(current == "alley" && val == "building"));
    }
}

void CityGeneration::drawRestrictedLine(int x0, int y0, int x1, int y1, const String &val, double cx, double cy, double a1, double a2, double r1, double r2) {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    while (true) {
        if (isInSector(x0, y0, cx, cy, a1, a2, r1, r2)) {
            if (canPlacePixel(x0, y0, val)) {
                canvas.setPixel(x0, y0, val);
            }
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;

        // Cardinal connectivity fix
        if (e2 >= dy && e2 <= dx) {
            int cx0 = x0, cy0 = y0;
            if (dx > -dy) cx0 += sx; else cy0 += sy;
            if (isInSector(cx0, cy0, cx, cy, a1, a2, r1, r2)) {
                if (canPlacePixel(cx0, cy0, val)) {
                    canvas.setPixel(cx0, cy0, val);
                }
            }
        }

        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void CityGeneration::splitSector(int x, int y, int w, int h, int depth, double cx, double cy, double a1, double a2, double r1, double r2) {
    if (depth <= 0 || w < 5 || h < 5) return;
    if (w > h) {
        int sx = x + static_cast<int>(w * (0.35 + randomDouble() * 0.3));
        drawRestrictedLine(sx, y, sx, y + h, "alley", cx, cy, a1, a2, r1, r2);
        splitSector(x, y, sx - x, h, depth - 1, cx, cy, a1, a2, r1, r2);
        splitSector(sx + 1, y, x + w - sx - 1, h, depth - 1, cx, cy, a1, a2, r1, r2);
    } else {
        int sy = y + static_cast<int>(h * (0.35 + randomDouble() * 0.3));
        drawRestrictedLine(x, sy, x + w, sy, "alley", cx, cy, a1, a2, r1, r2);
        splitSector(x, y, w, sy - y, depth - 1, cx, cy, a1, a2, r1, r2);
        splitSector(x, sy + 1, w, y + h - sy - 1, depth - 1, cx, cy, a1, a2, r1, r2);
    }
}

void CityGeneration::subdivideSector(double cx, double cy, double a1, double a2, double r1, double r2, int depth) {
    double start = a1, end = a2;
    if (std::abs(end - start) > Math_PI) {
        if (start < end) start += Math_PI * 2.0; else end += Math_PI * 2.0;
    }
    double corners[4][2] = {
        {cx + std::cos(start) * r1, cy + std::sin(start) * r1},
        {cx + std::cos(end) * r1, cy + std::sin(end) * r1},
        {cx + std::cos(start) * r2, cy + std::sin(start) * r2},
        {cx + std::cos(end) * r2, cy + std::sin(end) * r2}
    };
    double minX = corners[0][0], minY = corners[0][1], maxX = corners[0][0], maxY = corners[0][1];
    for (int i = 1; i < 4; ++i) {
        minX = std::min(minX, corners[i][0]); minY = std::min(minY, corners[i][1]);
        maxX = std::max(maxX, corners[i][0]); maxY = std::max(maxY, corners[i][1]);
    }
    splitSector(std::floor(minX), std::floor(minY), std::ceil(maxX - minX), std::ceil(maxY - minY), depth, cx, cy, a1, a2, r1, r2);
}

void CityGeneration::drawEmptyMarketSquare(double cx, double cy, double angle, int w, int h) {
    double cosA = std::cos(-angle), sinA = std::sin(-angle);
    double halfW = w / 2.0, halfH = h / 2.0;
    double diagonal = std::hypot(halfW, halfH);
    int startY = std::floor(cy - diagonal), endY = std::ceil(cy + diagonal);
    int startX = std::floor(cx - diagonal), endX = std::ceil(cx + diagonal);

    int gridSize = canvas.get_grid_size();
    for (int y = startY; y <= endY; ++y) {
        for (int x = startX; x <= endX; ++x) {
            if (y < 0 || y >= gridSize || x < 0 || x >= gridSize) continue;
            double dx = x - cx, dy = y - cy;
            double localX = dx * cosA - dy * sinA;
            double localY = dx * sinA + dy * cosA;
            if (std::abs(localX) <= halfW && std::abs(localY) <= halfH) {
                String current = canvas.getPixel(x, y);
                if (current != "water" && current != "palace") {
                    canvas.setPixel(x, y, "plains");
                }
            }
        }
    }
    double rCos = std::cos(angle), rSin = std::sin(angle);
    Point corners[4] = {{-w/2, -h/2}, {w/2, -h/2}, {w/2, h/2}, {-w/2, h/2}};
    for (int i = 0; i < 4; ++i) {
        int px1 = std::round(cx + corners[i].x * rCos - corners[i].y * rSin);
        int py1 = std::round(cy + corners[i].x * rSin + corners[i].y * rCos);
        int next = (i + 1) % 4;
        int px2 = std::round(cx + corners[next].x * rCos - corners[next].y * rSin);
        int py2 = std::round(cy + corners[next].x * rSin + corners[next].y * rCos);
        canvas.drawLine(px1, py1, px2, py2, "road");
    }
}

void CityGeneration::drawEmptyGrandPlaza(double cx, double cy, double r) {
    int gridSize = canvas.get_grid_size();
    for (int y = std::floor(cy - r - 1); y <= std::ceil(cy + r + 1); ++y) {
        for (int x = std::floor(cx - r - 1); x <= std::ceil(cx + r + 1); ++x) {
            if (y < 0 || y >= gridSize || x < 0 || x >= gridSize) continue;
            double dist = std::hypot(x - cx, y - cy);
            String current = canvas.getPixel(x, y);
            if (dist <= r + 0.5 && current != "water" && current != "palace") {
                canvas.setPixel(x, y, "plaza");
            }
        }
    }
    canvas.drawCircle(cx, cy, r, "road");
}

void CityGeneration::generateOuterDistricts(double cx, double cy, const std::vector<CityNode>& startNodes, int reach, int density) {
    int numRings = std::max(1, static_cast<int>(std::floor(reach / 18.0)));
    double stepLen = static_cast<double>(reach) / numRings;
    std::vector<CityNode> previousLayer = startNodes;

    for (int r = 1; r <= numRings; ++r) {
        std::vector<CityNode> currentLayer;
        if (previousLayer.empty()) break;
        double currentRadius = std::hypot(previousLayer[0].x - cx, previousLayer[0].y - cy) + stepLen;
        
        for (size_t i = 0; i < startNodes.size(); ++i) {
            int tx = std::round(cx + std::cos(startNodes[i].angle) * currentRadius);
            int ty = std::round(cy + std::sin(startNodes[i].angle) * currentRadius);
            canvas.drawLine(previousLayer[i].x, previousLayer[i].y, tx, ty, "road");
            currentLayer.push_back({tx, ty, startNodes[i].angle});
        }

        for (size_t i = 0; i < currentLayer.size(); ++i) {
            const CityNode& p1 = currentLayer[i];
            const CityNode& p2 = currentLayer[(i + 1) % currentLayer.size()];
            
            // Re-implementing drawOrganicArc using midpoints
            int mx = (p1.x + p2.x) / 2;
            int my = (p1.y + p2.y) / 2;
            std::uniform_int_distribution<int> dist(-1, 1);
            mx += dist(rng);
            my += dist(rng);
            canvas.drawLine(p1.x, p1.y, mx, my, "road");
            canvas.drawLine(mx, my, p2.x, p2.y, "road");

            double rIn = std::hypot(previousLayer[i].x - cx, previousLayer[i].y - cy);
            subdivideSector(cx, cy, p1.angle, p2.angle, rIn, currentRadius, density);
        }
        previousLayer = currentLayer;
    }
}

bool CityGeneration::isNearAnyRoad(int x, int y, int range) {
    int gridSize = canvas.get_grid_size();
    for (int iy = -range; iy <= range; ++iy) {
        for (int ix = -range; ix <= range; ++ix) {
            int ny = y + iy, nx = x + ix;
            if (ny >= 0 && ny < gridSize && nx >= 0 && nx < gridSize) {
                String t = canvas.getPixel(nx, ny);
                if (t == "road" || t == "alley" || t == "plaza") return true;
            }
        }
    }
    return false;
}

void CityGeneration::generateCity(
        double centerX, double centerY,
        int radius, int spokes, int rings, 
        int outerReach, int outerComp, int innerComp,
        bool showInner, bool showTwin,
        int tRadius, int tDensity, int tSpokes, int tRings,
        bool useRiver, bool useJitter, bool useSpecial
        ) 
    {
    
    canvas.clear("");
    int gridSize = canvas.get_grid_size();

    int gateCount = showInner ? spokes : 6;
    double gateRadius = showInner ? static_cast<double>(radius) : 2.5;
    std::vector<CityNode> gateCoords;

    for (int i = 0; i < gateCount; ++i) {
        double jitter = useJitter ? (spokeJitters[i % spokeJitters.size()]) * (Math_PI / (gateCount * 1.5)) : 0;
        double angle = (i * 2.0 * Math_PI) / gateCount + jitter;
        int gx = std::round(centerX + std::cos(angle) * gateRadius);
        int gy = std::round(centerY + std::sin(angle) * gateRadius);
        gateCoords.push_back({gx, gy, angle});
    }

    // Inner City
    if (showInner) {
        std::vector<int> computedRingRadii;
        for (int r = 1; r <= rings; ++r) {
            computedRingRadii.push_back(std::round(radius * std::pow(static_cast<double>(r) / rings, 0.8)));
        }
        for (const auto& gate : gateCoords) {
            canvas.drawLine(std::round(centerX), std::round(centerY), gate.x, gate.y, "road");
        }
        for (size_t rIdx = 0; rIdx < computedRingRadii.size(); ++rIdx) {
            canvas.drawCircle(centerX, centerY, computedRingRadii[rIdx], "road");
            if (rIdx == computedRingRadii.size() - 1) {
                for (const auto& g : gateCoords) canvas.fillRect(g.x - 1, g.y - 1, 3, 3, "gate");
            }
        }
        for (int i = 0; i < gateCount; ++i) {
            double a1 = gateCoords[i].angle, a2 = gateCoords[(i + 1) % gateCount].angle;
            for (int r = 0; r < rings; ++r) {
                double r1 = (r == 0 ? 8.0 : computedRingRadii[r - 1]);
                double r2 = computedRingRadii[r];
                subdivideSector(centerX, centerY, a1, a2, r1, r2, innerComp);
            }
        }
    } else {
        for (const auto& gate : gateCoords) {
            canvas.drawLine(std::round(centerX), std::round(centerY), gate.x, gate.y, "road");
        }
    }

    // Central Palace
    if (!showInner) {
        canvas.fillRect(static_cast<int>(centerX - 2), static_cast<int>(centerY - 2), 5, 5, "palace");
    } else {
        canvas.fillRect(static_cast<int>(std::round(centerX - 3)), static_cast<int>(std::round(centerY - 3)), 7, 7, "palace");
    }

    generateOuterDistricts(centerX, centerY, gateCoords, outerReach, outerComp);

    // Twin City
    if (showTwin) {
        double twinAngle = spawnRands[8] * Math_PI * 2.0;
        double twinDist = radius * 0.9 + 15.0;
        double tx = centerX + std::cos(twinAngle) * twinDist;
        double ty = centerY + std::sin(twinAngle) * twinDist;

        // 1. Clearance
        for (int y = std::floor(ty - tRadius - 8); y <= std::ceil(ty + tRadius + 8); ++y) {
            for (int x = std::floor(tx - tRadius - 8); x <= std::ceil(tx + tRadius + 8); ++x) {
                if (y < 0 || y >= gridSize || x < 0 || x >= gridSize) continue;
                double d = std::hypot(x - tx, y - ty);
                if (d <= tRadius + 2.0) {
                    String current = canvas.getPixel(x, y);
                    if (current != "water" && current != "palace") {
                        canvas.setPixel(x, y, "");
                    }
                }
            }
        }

        // 2. Twin Palace
        canvas.fillRect(std::round(tx - 2), std::round(ty - 2), 5, 5, "palace");

        // 3. Ring Roads
        std::vector<int> tRingRadii;
        for (int r = 1; r <= tRings; ++r) {
            int rDist = std::round(tRadius * (static_cast<double>(r) / tRings));
            tRingRadii.push_back(rDist);
            canvas.drawCircle(tx, ty, rDist, "road");
        }

        // 4. Boulevards
        std::vector<double> tAngles;
        for (int i = 0; i < tSpokes; ++i) {
            double a = (i * Math_PI * 2.0) / tSpokes;
            tAngles.push_back(a);
            int endX = std::round(tx + std::cos(a) * tRadius);
            int endY = std::round(ty + std::sin(a) * tRadius);
            canvas.drawLine(std::round(tx), std::round(ty), endX, endY, "road");
        }

        for (int i = 0; i < tSpokes; ++i) {
            double a1 = tAngles[i], a2 = tAngles[(i + 1) % tSpokes];
            for (int r = 0; r < tRings; ++r) {
                double r1 = (r == 0 ? 5.0 : tRingRadii[r - 1]);
                double r2 = tRingRadii[r];
                subdivideSector(tx, ty, a1, a2, r1, r2, tDensity);
            }
        }
    }

    // Special Districts
    if (showInner && useSpecial) {
        if (spawnRands[0] > 0.4 && !gateCoords.empty()) {
            int spokeIdx = std::floor(spawnRands[1] * gateCoords.size());
            double angle = gateCoords[spokeIdx].angle;
            double dist = radius * 0.55;
            double mx = centerX + std::cos(angle) * dist;
            double my = centerY + std::sin(angle) * dist;
            drawEmptyMarketSquare(mx, my, angle, 9, 9);
        }
        if (spawnRands[2] > 0.3) {
            double pAngle = (spawnRands[3] + 0.1) * Math_PI * 2.0;
            double pDist = radius * 0.75;
            double px = centerX + std::cos(pAngle) * pDist;
            double py = centerY + std::sin(pAngle) * pDist;
            drawEmptyGrandPlaza(px, py, 5);
        }
    }

    // Final Building Pass
    for (int y = 0; y < gridSize; ++y) {
        for (int x = 0; x < gridSize; ++x) {
            String current = canvas.getPixel(x, y);
            if (current == "") {
                double dist = std::hypot(x - centerX, y - centerY);
                if (dist > 2.0 && dist < gridSize * 0.49) {
                    if (isNearAnyRoad(x, y, 1)) {
                        if (randomDouble() > 0.2) canvas.setPixel(x, y, "building");
                    }
                }
            }
        }
    }
}

namespace {
    constexpr int MIN_CITY_SIZE = 24;
    constexpr int MAX_CITY_SIZE = 48;
    constexpr int PHASE_TRANSITION_SIZE = 32;

    constexpr int MIN_SPOKES = 4; // Road boulevards connecting from palace to outer city
    constexpr int MAX_SPOKES = 8;
    constexpr int MIN_RINGS = 1; // Road rings circling the palace
    constexpr int MAX_RINGS = 3;

    constexpr int RADIUS_JITTER = 2;
    constexpr int SPOKE_JITTER = 1;

    constexpr int DEFAULT_DENSITY = 5; // Alley density

    constexpr int SHOW_TWIN = false;
    // TODO: Twin city generation
    
    constexpr int USE_RIVER = false;
    constexpr int USE_JITTER = true;
    constexpr int USE_SPECIAL = true;
}

void CityGeneration::spawn_city(Canvas& p_canvas, int x, int y, int world_seed) {
    // Deterministic city seed using prime-based hashing
    const uint32_t city_seed = static_cast<uint32_t>(world_seed) + (x * 31) + (y * 7);
    
    std::mt19937 city_rng(city_seed);
    
    // Determine base city size
    std::uniform_int_distribution<int> size_dist(MIN_CITY_SIZE, MAX_CITY_SIZE);
    const int city_size = size_dist(city_rng);

    // Calculate phased growth parameters
    int reach, radius, spokes, rings;
    bool show_inner;

    // Cities always contain outer city area
    if (city_size <= PHASE_TRANSITION_SIZE) {
        show_inner = false;
        reach = city_size;
        radius = MIN_CITY_SIZE;
        spokes = MIN_SPOKES;
        rings = MIN_RINGS;
    } else {
    // Cities over a certain size contain inner city area
        show_inner = true;
        
        const int size_overflow = city_size - PHASE_TRANSITION_SIZE;
        const float growth_progress = static_cast<float>(size_overflow) / (MAX_CITY_SIZE - PHASE_TRANSITION_SIZE);
        
        reach = MIN_CITY_SIZE + size_overflow; // Scales 24 -> 40
        radius = MIN_CITY_SIZE + static_cast<int>(std::round(growth_progress * (MAX_CITY_SIZE - MIN_CITY_SIZE)));
        spokes = MIN_SPOKES + static_cast<int>(std::round(growth_progress * (MAX_SPOKES - MIN_SPOKES)));
        rings = MIN_RINGS + static_cast<int>(std::round(growth_progress * (MAX_RINGS - MIN_RINGS)));
    }
    
    // Apply deterministic jitter for individual variety
    std::uniform_int_distribution<int> radius_jitter_dist(-RADIUS_JITTER, RADIUS_JITTER);
    std::uniform_int_distribution<int> spokes_jitter_dist(-SPOKE_JITTER, SPOKE_JITTER);
    
    radius = std::clamp(radius + radius_jitter_dist(city_rng), MIN_CITY_SIZE, MAX_CITY_SIZE);
    spokes = std::clamp(spokes + spokes_jitter_dist(city_rng), MIN_SPOKES, MAX_SPOKES);
    rings = std::clamp(rings, MIN_RINGS, MAX_RINGS);

    // Initialize and generate the city
    CityGeneration gen(p_canvas, city_seed);
    gen.generateCity(
        static_cast<double>(x), static_cast<double>(y),
        radius, spokes, rings, 
        reach, DEFAULT_DENSITY, DEFAULT_DENSITY, 
        show_inner, SHOW_TWIN,
        30, 4, 6, 2, // twinRadius, twinDensity, twinSpokes, twinRings
        USE_RIVER, USE_JITTER, USE_SPECIAL
    );
    
    UtilityFunctions::print("City generated at (", x, ", ", y, ") | Size: ", city_size, " | Type: ", show_inner ? "Metropolis" : "Outpost");
}

String CityGeneration::get_chunk_id(const String &p_tile) {
    if (p_tile == "") return (UtilityFunctions::randi() % 2 == 0) ? "forest" : "plains";
    return p_tile;
}

}
