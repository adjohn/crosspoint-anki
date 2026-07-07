#pragma once
#include <WString.h>
#include <cstdint>
#include <map>
#include <ArduinoJson.h>

struct CardProgress {
    float ease;
    int interval;     // days
    int repetitions;
    int32_t due;      // epoch days (days since 1970-01-01); 0 = always due

    CardProgress() : ease(2.5f), interval(0), repetitions(0), due(0) {}
};

struct DeckProgress {
    String deckId;
    int32_t lastReview;  // epoch days; 0 = never reviewed / unknown
    std::map<String, CardProgress> cards;

    DeckProgress() : lastReview(0) {}

    static DeckProgress fromJson(const String& json) {
        DeckProgress progress;
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, json);

        if (!error) {
            progress.deckId = doc["deckId"] | "";
            // Older files stored dates as strings; `| 0` migrates them to
            // "due immediately" / "never reviewed"
            progress.lastReview = doc["lastReview"] | 0;

            JsonObject cardsObj = doc["cards"];
            for (JsonPair p : cardsObj) {
                CardProgress cp;
                cp.ease = p.value()["ease"] | 2.5f;
                cp.interval = p.value()["interval"] | 0;
                cp.repetitions = p.value()["repetitions"] | 0;
                cp.due = p.value()["due"] | 0;
                progress.cards[p.key().c_str()] = cp;
            }
        }
        return progress;
    }

    String toJson() const {
        JsonDocument doc;
        doc["deckId"] = deckId;
        doc["lastReview"] = lastReview;

        JsonObject cardsObj = doc["cards"].to<JsonObject>();
        for (auto const& pair : cards) {
            const String& id = pair.first;
            const CardProgress& cp = pair.second;
            JsonObject cpObj = cardsObj[id].to<JsonObject>();
            cpObj["ease"] = cp.ease;
            cpObj["interval"] = cp.interval;
            cpObj["repetitions"] = cp.repetitions;
            cpObj["due"] = cp.due;
        }

        String output;
        serializeJson(doc, output);
        return output;
    }

    CardProgress& getCardProgress(const String& cardId) {
        return cards[cardId];
    }

    // Number of cards due on `today` (epoch days): tracked cards with
    // due <= today plus new cards the deck holds beyond the tracked ones.
    // today < 0 (no clock) counts everything as due. Progress-only — never
    // touches cards.jsonl.
    int countDue(int32_t today, int totalCardCount) const {
        int due = totalCardCount - static_cast<int>(cards.size());
        if (due < 0) {
            due = 0;
        }
        for (auto const& pair : cards) {
            if (today < 0 || pair.second.due <= today) {
                due++;
            }
        }
        return due;
    }
};
