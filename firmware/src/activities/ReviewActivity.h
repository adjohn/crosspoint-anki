#pragma once

#include "Activity.h"
#include "../data/Card.h"
#include "../data/Progress.h"
#include "../storage/DeckStorage.h"
#include "../scheduling/SM2.h"
#include "../scheduling/RelearnQueue.h"
#include "../utils/TextWrap.h"
#include <GfxRenderer.h>
#include <MappedInputManager.h>
#include <cstdint>
#include <vector>

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
    int reviewedCount;   // UNIQUE cards rated this session (relearn re-ratings don't count)
    int dueTotal;        // cards due today at session start
    int32_t today;       // epoch days, -1 when no trustworthy clock
    RelearnQueue relearnQueue;   // Again-rated card IDs pending in-session relearning
    bool inRelearnPass;          // main due-pass done; stream now serves queued cards only
    bool relearnServedThisPass;  // loop guard: a pass that serves nothing ends the session

    // Long-card pagination: line offsets into currentCard.front/back (no copies)
    std::vector<TextWrap::Line> frontLines;
    std::vector<TextWrap::Line> backLines;
    int page;       // 0-based page of the active state's paginated text
    int pageCount;  // pages for the active state (>= 1)

    void showFront();
    void showBack();
    void showRating();
    void showFinished();
    void renderFront();
    void renderBack();
    void drawHeader();
    void drawPageIndicator();
    void drawWrappedLines(const String& text, const std::vector<TextWrap::Line>& lines,
                          int firstLine, int maxLines, int yTop);
    void changePage(int delta);

    int wrapWidth() const;
    int frontLinesPerPage() const;
    int backLinesPerPage() const;
    int frontSummaryMaxLines() const;

    void processRating(SM2::Quality quality);
    bool isCardDue(const String& cardId) const;
    bool advanceToNextCard();
    bool startRelearnPass();
};
