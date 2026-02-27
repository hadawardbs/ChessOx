#ifndef OXTA_TUNING_SPSA_HPP
#define OXTA_TUNING_SPSA_HPP

#include "../core/eval.hpp"
#include <vector>
#include <iostream>
#include <random>

namespace Oxta::Tuning {

struct Param {
    int* value;
    int min;
    int max;
    const char* name;
};

class SPSA {
public:
    void add_param(int& var, int min, int max, const char* name) {
        params.push_back({&var, min, max, name});
    }

    // Stub for SPSA Logic
    // In a real tuner, this would:
    // 1. Perturb parameters (+/- delta)
    // 2. Run match (Self-Play)
    // 3. Update parameters based on result (Gradient)

    void run_iteration() {
        std::cout << "SPSA: Tuning " << params.size() << " parameters..." << std::endl;
        // ... Logic ...
        std::cout << "SPSA: Iteration complete." << std::endl;
    }

private:
    std::vector<Param> params;
};

} // namespace Oxta::Tuning

#endif // OXTA_TUNING_SPSA_HPP
