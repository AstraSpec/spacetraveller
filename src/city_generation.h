#ifndef SPACETRAVELLER_CITY_GENERATION_H
#define SPACETRAVELLER_CITY_GENERATION_H

#include "canvas.h"
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/core/math.hpp>
#include <vector>
#include <random>

namespace godot {

struct CityNode {
    int x, y;
    double angle;
};

class CityGeneration {
private:
    Canvas& canvas;
    std::vector<double> spokeJitters;
    std::vector<double> spawnRands;
    
    std::mt19937 rng;

    uint16_t id_road;
    uint16_t id_alley;
    uint16_t id_building;
    uint16_t id_palace;
    uint16_t id_water;
    uint16_t id_gate;
    uint16_t id_plaza;
    uint16_t id_forest;
    uint16_t id_plains;
    uint16_t id_void;

    class IdRegistry* registry;

    double randomDouble();
    void randomize();
    
    bool isInSector(int px, int py, double cx, double cy, double a1, double a2, double r1, double r2);
    bool canPlacePixel(int x, int y, uint16_t val_id);
    void drawRestrictedLine(int x0, int y0, int x1, int y1, uint16_t val_id, double cx, double cy, double a1, double a2, double r1, double r2, uint8_t p_meta = 0);
    void splitSector(int x, int y, int w, int h, int depth, double cx, double cy, double a1, double a2, double r1, double r2);
    void subdivideSector(double cx, double cy, double a1, double a2, double r1, double r2, int depth);
    void drawEmptyMarketSquare(double cx, double cy, double angle, int w, int h);
    void drawEmptyGrandPlaza(double cx, double cy, double r);
    void generateOuterDistricts(double cx, double cy, const std::vector<CityNode>& startNodes, int reach, int density);
    bool isNearAnyRoad(int x, int y, int range);

public:
    CityGeneration(Canvas& p_canvas, uint32_t seed, class IdRegistry* p_registry);
    
    void generateCity(
        double centerX, double centerY,
        int radius, int spokes, int rings, 
        int outerReach, int outerComp, int innerComp,
        bool showInner, bool showTwin,
        int tRadius, int tDensity, int tSpokes, int tRings,
        bool useRiver, bool useJitter, bool useSpecial
    );

    static void spawn_city(Canvas& p_canvas, int x, int y, int world_seed);
};

}

#endif // SPACETRAVELLER_CITY_GENERATION_H
