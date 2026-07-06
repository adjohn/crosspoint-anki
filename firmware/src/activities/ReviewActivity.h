#pragma once

#include "Activity.h"
#include "../data/Card.h"
#include "../data/Progress.h"
#include "../storage/DeckStorage.h"
#include "../scheduling/SM2.h"
#include <GfxRenderer.h>
#include <MappedInputManager.h>
#include <cstdint>

class ReviewActivity : public Activity {
public:
    enum State {
        SHOWING_FRONT,
        SHOWING_BACK,
        RATING,
        FINISHED
    };

    ReviewActivity(String deckId, GfxRenderer& renderer, MappedInputManager& input, bool rtcAvailable);
    virtual ~ReviewActivity() = default;

    void onEnter() override;
    void onExit() override;
    void loop() override;

private:
    String deckId;
    State currentState;
    CardStream cardStream;
    Card currentCard;
    DeckProgress deckProgress;
    bool rtcAvailable;
    bool hasMoreCards;
    int reviewedCount;
    int dueTotal;        // cards due today at session start
    int32_t today;       // epoch days, -1 when no trustworthy clock

    void showFront();
    void showBack();
    void showRating();
    void showFinished();

    void processRating(SM2::Quality quality);
    bool isCardDue(const String& cardId) const;
    bool advanceToNextDueCard();
    void drawCardContent(const String& content, const char* title);
};
