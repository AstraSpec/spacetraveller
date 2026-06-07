#ifndef SPACETRAVELLER_RNG_H
#define SPACETRAVELLER_RNG_H

#include <godot_cpp/variant/vector2i.hpp>
#include <cstdint>

namespace godot {

namespace Rng {
    // Independent streams so different attributes derived at the same
    // position with the same world seed do not correlate with each other.
    enum Stream : uint32_t {
        TILE_VARIANT = 1,
        GENDER = 2,
        NAME = 3,
        LOOT = 4,
        BIOME = 5,
        SPAWN = 6,
        SPAWN_RULE = 7,
        SPAWN_LOOT = 8,
        TILE_LOOT = 9,
        ENTITY_LOOT = 10,
        CONTAINER_LOOT = 11,
        VENDOR_LOOT = 12,
        QUEST_LOOT = 13,
    };

    inline uint64_t mix64(uint64_t z) {
        z += 0x9E3779B97F4A7C15ULL;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    struct Seeded {
        uint64_t state;

        uint32_t next_u32() {
            state = mix64(state);
            return static_cast<uint32_t>(state >> 32);
        }

        // Inclusive on both ends.
        int range(int lo, int hi) {
            if (hi <= lo) return lo;
            uint32_t span = static_cast<uint32_t>(hi - lo + 1);
            return lo + static_cast<int>(next_u32() % span);
        }

        // [0, 1)
        float unit() {
            return static_cast<float>(next_u32() >> 8) / static_cast<float>(1u << 24);
        }

        bool chance(float p) {
            return unit() < p;
        }
    };

    inline uint64_t hash_pos(uint32_t world_seed, const Vector2i& pos, Stream stream) {
        uint64_t h = mix64(static_cast<uint64_t>(world_seed) ^ (static_cast<uint64_t>(stream) << 56));
        h = mix64(h ^ static_cast<uint64_t>(static_cast<uint32_t>(pos.x)));
        h = mix64(h ^ (static_cast<uint64_t>(static_cast<uint32_t>(pos.y)) << 32));
        return h;
    }

    inline Seeded at(uint32_t world_seed, const Vector2i& pos, Stream stream) {
        return Seeded{ hash_pos(world_seed, pos, stream) };
    }

    inline Seeded at(uint32_t world_seed, const Vector2i& pos, Stream stream, uint64_t salt) {
        return Seeded{ mix64(hash_pos(world_seed, pos, stream) ^ salt) };
    }

    // Stateless variant index for callers that only need a single deterministic
    // pick from a position (e.g. tile atlas variants) without a Seeded object.
    inline uint32_t variant_index(uint32_t world_seed, int x, int y, uint32_t count) {
        if (count <= 1) return 0;
        return static_cast<uint32_t>(hash_pos(world_seed, Vector2i(x, y), TILE_VARIANT) % count);
    }
}

}

#endif // SPACETRAVELLER_RNG_H
