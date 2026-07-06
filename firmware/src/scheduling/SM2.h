#pragma once

namespace SM2 {
    // Canonical incremental SM-2 interval. repetitions is the count AFTER
    // the current review (i.e. update repetitions first, then call this):
    //   reps <= 0 (lapse)  -> 1 day (restart tomorrow)
    //   reps == 1          -> 1 day
    //   reps == 2          -> 6 days
    //   reps  > 2          -> round(prevInterval * easeFactor), never
    //                         shrinking below prevInterval + 1
    int calculateInterval(int repetitions, int prevInterval, float easeFactor);

    float updateEaseFactor(float currentEF, int quality);

    enum Quality {
        AGAIN = 0,
        HARD = 2,
        GOOD = 3,
        EASY = 5
    };
}
