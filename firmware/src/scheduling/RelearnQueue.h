#pragma once

#include <WString.h>
#include <vector>
#include "SM2.h"

// In-session relearning: a card rated Again comes back later in the SAME
// session until it earns Good/Easy. Hard is a lapse but is scheduled out to
// tomorrow, so it leaves the queue. Pure logic (String + vector only) so the
// native host tests can exercise it.
class RelearnQueue {
public:
    // Bounded so a pathological session can't grow the queue without limit.
    // Cards rejected at the cap stay due today and the next session catches
    // them.
    static constexpr size_t MAX_IDS = 256;

    bool contains(const String& id) const {
        for (const String& queued : ids) {
            if (queued == id) {
                return true;
            }
        }
        return false;
    }

    // Fold a rating into the queue: Again enqueues (capacity permitting),
    // every other rating dequeues the card if present.
    void onRated(const String& id, SM2::Quality quality) {
        if (quality == SM2::AGAIN) {
            if (!contains(id) && ids.size() < MAX_IDS) {
                ids.push_back(id);
            }
            return;
        }
        for (size_t i = 0; i < ids.size(); i++) {
            if (ids[i] == id) {
                ids.erase(ids.begin() + i);
                return;
            }
        }
    }

    bool empty() const { return ids.empty(); }
    size_t size() const { return ids.size(); }

private:
    std::vector<String> ids;
};

namespace Relearn {
    // Again-rated cards stay due today so they remain due if the user exits
    // mid-session; every other rating pushes due out by the new interval.
    inline bool staysDueToday(SM2::Quality quality) {
        return quality == SM2::AGAIN;
    }
}
