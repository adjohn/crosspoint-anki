#include "SM2.h"
#include <cmath>

namespace SM2 {

int calculateInterval(int repetitions, int prevInterval, float easeFactor) {
    if (repetitions <= 1) {
        return 1;
    }
    if (repetitions == 2) {
        return 6;
    }

    int next = static_cast<int>(std::round(prevInterval * easeFactor));
    // EF >= 1.3 keeps this monotonic for sane inputs; the guard also heals
    // progress entries migrated with interval 0 at high repetition counts.
    if (next <= prevInterval) {
        next = prevInterval + 1;
    }
    return next;
}

float updateEaseFactor(float currentEF, int quality) {
    float q = static_cast<float>(quality);
    float newEF = currentEF + (0.1f - (5.0f - q) * (0.08f + (5.0f - q) * 0.02f));

    if (newEF < 1.3f) {
        newEF = 1.3f;
    }

    return newEF;
}

}
