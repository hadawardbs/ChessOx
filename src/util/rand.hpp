#ifndef OXTA_UTIL_RAND_HPP
#define OXTA_UTIL_RAND_HPP

#include <cstdint>
#include <limits>

namespace Oxta::Util {

// Xoroshiro128+ implementation
// A fast, high-quality PRNG suitable for Zobrist hashing and MCTS
class PRNG {
public:
    explicit PRNG(uint64_t seed = 0xDEADBEEFCAFEBABE) {
        s[0] = seed;
        s[1] = seed ^ 0xF00D159623547809; // Simple initial mix
        // Warm up
        for (int i = 0; i < 10; ++i) next();
    }

    uint64_t next() {
        const uint64_t s0 = s[0];
        uint64_t s1 = s[1];
        const uint64_t result = s0 + s1;

        s1 ^= s0;
        s[0] = rotl(s0, 24) ^ s1 ^ (s1 << 16); // a, b
        s[1] = rotl(s1, 37); // c

        return result;
    }

    // Generate a random number in [0, max]
    uint64_t rand_range(uint64_t max) {
        return next() % (max + 1);
    }

    // Generate a double in [0, 1)
    double rand_double() {
        return (next() >> 11) * (1.0 / 9007199254740992.0);
    }

private:
    uint64_t s[2];

    static inline uint64_t rotl(const uint64_t x, int k) {
        return (x << k) | (x >> (64 - k));
    }
};

} // namespace Oxta::Util

#endif // OXTA_UTIL_RAND_HPP
