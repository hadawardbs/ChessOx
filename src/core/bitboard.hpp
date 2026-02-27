#ifndef OXTA_CORE_BITBOARD_HPP
#define OXTA_CORE_BITBOARD_HPP

#include "types.hpp"
#include <bit>
#include <concepts>
#include <immintrin.h> // For _pext_u64

namespace Oxta::Bitboards {

// Using C++20 <bit> header for portable and efficient bit manipulation
// Compilers like GCC/Clang will optimize these to BSF/BSR/POPCNT instructions

inline int popcount(Bitboard b) {
    return std::popcount(b);
}

// Least Significant Bit index (0-63). Undefined for b == 0.
inline Square lsb(Bitboard b) {
    assert(b != 0);
    return static_cast<Square>(std::countr_zero(b));
}

// Most Significant Bit index (0-63). Undefined for b == 0.
inline Square msb(Bitboard b) {
    assert(b != 0);
    return static_cast<Square>(63 - std::countl_zero(b));
}

inline void set_bit(Bitboard& b, Square s) {
    b |= (1ULL << s);
}

inline void clear_bit(Bitboard& b, Square s) {
    b &= ~(1ULL << s);
}

inline bool check_bit(Bitboard b, Square s) {
    return (b & (1ULL << s)) != 0;
}

// Reset the least significant bit and return its index
inline Square pop_lsb(Bitboard& b) {
    Square s = lsb(b);
    b &= b - 1;
    return s;
}

// PEXT wrapper for Magic Bitboards / Sliding Attacks
inline Bitboard pext(Bitboard val, Bitboard mask) {
#if defined(__BMI2__)
    return _pext_u64(val, mask);
#else
    Bitboard res = 0;
    for (Bitboard bb = 1; mask != 0; bb += bb) {
        if (val & mask & -mask)
            res |= bb;
        mask &= mask - 1;
    }
    return res;
#endif
}

} // namespace Oxta::Bitboards

#endif // OXTA_CORE_BITBOARD_HPP
