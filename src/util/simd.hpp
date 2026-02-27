#ifndef OXTA_UTIL_SIMD_HPP
#define OXTA_UTIL_SIMD_HPP

#include <iostream>

// Simple detection based on compiler macros
#if defined(__AVX512F__)
    #define OXTA_SIMD_AVX512
    #define OXTA_SIMD_NAME "AVX512"
#elif defined(__AVX2__)
    #define OXTA_SIMD_AVX2
    #define OXTA_SIMD_NAME "AVX2"
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    #define OXTA_SIMD_NEON
    #define OXTA_SIMD_NAME "NEON"
#else
    #define OXTA_SIMD_SCALAR
    #define OXTA_SIMD_NAME "SCALAR"
#endif

namespace Oxta::Simd {

inline void print_simd_support() {
    std::cout << "Detected SIMD Architecture: " << OXTA_SIMD_NAME << std::endl;
}

} // namespace Oxta::Simd

#endif // OXTA_UTIL_SIMD_HPP
