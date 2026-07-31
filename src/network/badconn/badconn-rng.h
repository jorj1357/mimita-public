// 07 31 2026, 15 30
/* purpose
* Implements the small seeded RNG used for per-packet impairment rolls.
* Provides integer, percent, and unit-interval draws on one mt19937 generator.
* Keeps deterministic seeds available for tests while 0 seeds from random_device.
* Does NOT own preset state, queues, or impairment policy.
*/

#pragma once

#include <random>

namespace badconn {

inline void BadConnRng::seed(uint32_t seed)
{
    if (seed == 0)
        seed = std::random_device{}();
    mGen.seed(seed);
}

inline int BadConnRng::nextInt(int lowInclusive, int highInclusive)
{
    if (lowInclusive > highInclusive)
        return lowInclusive;
    std::uniform_int_distribution<int> distribution(lowInclusive, highInclusive);
    return distribution(mGen);
}

inline float BadConnRng::nextPercent()
{
    std::uniform_real_distribution<double> distribution(0.0, 100.0);
    return static_cast<float>(distribution(mGen));
}

inline double BadConnRng::nextUnit()
{
    std::uniform_real_distribution<double> distribution(0.0, 1.0);
    return distribution(mGen);
}

} // namespace badconn
