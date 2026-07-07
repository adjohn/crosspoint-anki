#include "ReviewActivity.h"
#include "../utils/TimeUtils.h"
#include <Arduino.h>
#include <string>

// Font constants - assuming these are available in GfxRenderer
// If not, we'll need to use what's available or load them
#define FONT_LARGE 4
#define FONT_MEDIUM 2
#define FONT_SMALL 1

ReviewActivity::ReviewActivity(String deckId, GfxRenderer& renderer, MappedInputManager& input, bool rtcAvailable)
    : Activity("Review", renderer, input),
      deckId(deckId),
      currentState(SHOWING_FRONT),
      rtcAvailable(rtcAvailable),
      hasMoreCards(true),
      reviewedCount(0),
      dueTotal(0),
      today(-1),
      inRelearnPass(false),
      relearnServedThisPass(false),
      page(0),
      pageCount(1) {
}

void ReviewActivity::onEnter() {
    Activity::onEnter();

    // Load progress and open stream
    deckProgress = DeckStorage::loadProgress(deckId);
    today = TimeUtils::todayEpochDays(rtcAvailable);
    dueTotal = deckProgress.countDue(today, DeckStorage::loadDeckMetadata(deckId).cardCount);

    if (DeckStorage::openCardStream(deckId, cardStream)) {
        // Get first due card (with no clock, every card is due)
        if (advanceToNextCard()) {
            hasMoreCards = true;
            showFront();
        } else {
            hasMoreCards = false;
            showFinished();
        }
    } else {
        Serial.println("Failed to open card stream");
        hasMoreCards = false;
        showFinished(); // Or error state
    }
}

void ReviewActivity::onExit() {
    // Each rating is saved as it happens; save again here so the exit
    // path persists progress even if that ever changes.
    if (reviewedCount > 0) {
        DeckStorage::saveProgress(deckId, deckProgress);
    }
    cardStream.close();
    Activity::onExit();
}

void ReviewActivity::loop() {
    // Tilt gestures are consume-on-read: read once per frame.
    // Reading them here also discards them in states that ignore tilt (FINISHED).
    const bool tiltForward = input.wasTiltedForward();
    const bool tiltBack = input.wasTiltedBack();

    // Handle input based on state
    if (currentState == FINISHED) {
        if (input.wasPressed(MappedInputManager::Button::Back) ||
            input.wasPressed(MappedInputManager::Button::Confirm)) {
            requestNav(NavTarget::DeckList);
        }
        return;
    }

    // Up/Down page through long cards while the card text is showing; tilt
    // pages until the boundary, where it falls through to its usual action.
    if (currentState == SHOWING_FRONT) {
        if (input.wasPressed(MappedInputManager::Button::Confirm)) {
            showBack();  // Confirm always reveals, whatever the page
        } else if (input.wasPressed(MappedInputManager::Button::Back)) {
            requestNav(NavTarget::DeckList);
        } else if (input.wasPressed(MappedInputManager::Button::Up)) {
            changePage(-1);
        } else if (input.wasPressed(MappedInputManager::Button::Down)) {
            changePage(1);
        } else if (tiltForward) {
            if (page < pageCount - 1) {
                changePage(1);
            } else {
                showBack();  // Last page: tilt forward reveals the answer
            }
        } else if (tiltBack) {
            changePage(-1);  // Page 1: no-op, as tilt back always was here
        }
    } else if (currentState == SHOWING_BACK) {
        if (input.wasPressed(MappedInputManager::Button::Confirm)) {
            showRating();
        } else if (input.wasPressed(MappedInputManager::Button::Back)) {
            requestNav(NavTarget::DeckList);
        } else if (input.wasPressed(MappedInputManager::Button::Up)) {
            changePage(-1);
        } else if (input.wasPressed(MappedInputManager::Button::Down)) {
            changePage(1);
        } else if (tiltForward) {
            if (page < pageCount - 1) {
                changePage(1);
            } else {
                processRating(SM2::GOOD);  // Last page: tilt forward rates Good
            }
        } else if (tiltBack) {
            if (page > 0) {
                changePage(-1);
            } else {
                processRating(SM2::AGAIN); // Page 1: tilt back rates Again
            }
        }
    } else if (currentState == RATING) {
        if (input.wasPressed(MappedInputManager::Button::Back)) {
             requestNav(NavTarget::DeckList);
        } else if (input.wasPressed(MappedInputManager::Button::Left)) {
             processRating(SM2::AGAIN); // Map Left to Again
        } else if (input.wasPressed(MappedInputManager::Button::Down)) {
             processRating(SM2::HARD);  // Map Down to Hard
        } else if (input.wasPressed(MappedInputManager::Button::Up)) {
             processRating(SM2::GOOD);  // Map Up to Good
        } else if (input.wasPressed(MappedInputManager::Button::Right)) {
             processRating(SM2::EASY);  // Map Right to Easy
        } else if (tiltForward) {
             processRating(SM2::GOOD);  // Tilt forward rates Good
        } else if (tiltBack) {
             processRating(SM2::AGAIN); // Tilt back rates Again
        }
    }
}

// Text layout: 10 px side margins; card text in FONT_MEDIUM. All geometry is
// runtime so the same code fits X4 (800x480) and X3 (792x528).
int ReviewActivity::wrapWidth() const {
    return renderer.getScreenWidth() - 20;
}

// Front state: text from y=60 down to just above the bottom hint line
int ReviewActivity::frontLinesPerPage() const {
    const int lh = renderer.getLineHeight(FONT_MEDIUM);
    const int area = renderer.getScreenHeight() - 40 - 60;
    const int n = (lh > 0) ? area / lh : 1;
    return n > 0 ? n : 1;
}

// Back state: text between the mid-screen divider and the rating-bar zone,
// so the RATING overlay (bottom 60 px) never covers card text
int ReviewActivity::backLinesPerPage() const {
    const int lh = renderer.getLineHeight(FONT_MEDIUM);
    const int h = renderer.getScreenHeight();
    const int area = (h - 70) - (h / 2 + 30);
    const int n = (lh > 0) ? area / lh : 1;
    return n > 0 ? n : 1;
}

// Front summary on the back screen: y=60 down to just above the divider
int ReviewActivity::frontSummaryMaxLines() const {
    const int lh = renderer.getLineHeight(FONT_MEDIUM);
    const int area = renderer.getScreenHeight() / 2 - 10 - 60;
    const int n = (lh > 0) ? area / lh : 1;
    return n > 0 ? n : 1;
}

void ReviewActivity::drawWrappedLines(const String& text, const std::vector<TextWrap::Line>& lines,
                                      int firstLine, int maxLines, int yTop) {
    const int lh = renderer.getLineHeight(FONT_MEDIUM);
    std::string buf;  // scratch for null-terminated line slices
    int y = yTop;
    const int end = firstLine + maxLines;
    for (int i = firstLine; i < (int)lines.size() && i < end; i++) {
        if (lines[i].length > 0) {
            buf.assign(text.c_str() + lines[i].start, lines[i].length);
            renderer.drawText(FONT_MEDIUM, 10, y, buf.c_str());
        }
        y += lh;
    }
}

void ReviewActivity::changePage(int delta) {
    int next = page + delta;
    if (next < 0) next = 0;
    if (next > pageCount - 1) next = pageCount - 1;
    if (next == page) {
        return;  // boundary: no redraw
    }
    page = next;
    if (currentState == SHOWING_FRONT) {
        renderFront();
    } else if (currentState == SHOWING_BACK) {
        renderBack();
    }
}

void ReviewActivity::showFront() {
    currentState = SHOWING_FRONT;
    TextWrap::wrap(renderer, FONT_MEDIUM, currentCard.front, wrapWidth(), frontLines);
    backLines.clear();  // back is wrapped on reveal
    const int perPage = frontLinesPerPage();
    pageCount = ((int)frontLines.size() + perPage - 1) / perPage;
    if (pageCount < 1) pageCount = 1;
    page = 0;
    renderFront();
}

void ReviewActivity::renderFront() {
    renderer.clearScreen(0xFF);

    drawHeader();
    drawPageIndicator();

    renderer.drawText(FONT_SMALL, 10, 40, "FRONT");
    drawWrappedLines(currentCard.front, frontLines, page * frontLinesPerPage(),
                     frontLinesPerPage(), 60);

    renderer.drawText(FONT_SMALL, 10, renderer.getScreenHeight() - 20,
                      pageCount > 1 ? "Confirm: reveal | Up/Down: page"
                                    : "Press Confirm to reveal");

    renderer.displayBuffer();
}

void ReviewActivity::showBack() {
    currentState = SHOWING_BACK;
    TextWrap::wrap(renderer, FONT_MEDIUM, currentCard.back, wrapWidth(), backLines);
    const int perPage = backLinesPerPage();
    pageCount = ((int)backLines.size() + perPage - 1) / perPage;
    if (pageCount < 1) pageCount = 1;
    page = 0;  // revealing always starts at page 1 of the back
    renderBack();
}

void ReviewActivity::renderBack() {
    renderer.clearScreen(0xFF);

    drawHeader();
    drawPageIndicator();

    // Front summary in the top half (clipped, not paginated)
    renderer.drawText(FONT_SMALL, 10, 40, "Front:");
    drawWrappedLines(currentCard.front, frontLines, 0, frontSummaryMaxLines(), 60);

    const int h = renderer.getScreenHeight();
    renderer.drawLine(0, h / 2, renderer.getScreenWidth(), h / 2);

    renderer.drawText(FONT_SMALL, 10, h / 2 + 10, "BACK");
    drawWrappedLines(currentCard.back, backLines, page * backLinesPerPage(),
                     backLinesPerPage(), h / 2 + 30);

    renderer.drawText(FONT_SMALL, 10, h - 20,
                      pageCount > 1 ? "Confirm: rate | Up/Down: page"
                                    : "Press Confirm to rate");

    renderer.displayBuffer();
}

void ReviewActivity::drawHeader() {
    // Subtle cue during relearn passes: title swaps to "Relearning"
    String header = inRelearnPass ? String("Relearning") : ("Review: " + deckId);
    renderer.drawText(FONT_SMALL, 10, 10, header.c_str());
    renderer.drawLine(0, 30, renderer.getScreenWidth(), 30);
}

// "2/5" right-aligned in the header row; only when the text spans pages
void ReviewActivity::drawPageIndicator() {
    if (pageCount <= 1) {
        return;
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%d/%d", page + 1, pageCount);
    const int tw = renderer.getTextWidth(FONT_SMALL, buf);
    renderer.drawText(FONT_SMALL, renderer.getScreenWidth() - 10 - tw, 10, buf);
}

void ReviewActivity::showRating() {
    currentState = RATING;
    // Overlay rating controls or redraw bottom area
    
    int y = renderer.getScreenHeight() - 60;
    renderer.fillRect(0, y, renderer.getScreenWidth(), 60, 0xFF);
    renderer.drawLine(0, y, renderer.getScreenWidth(), y);
    
    renderer.drawText(FONT_SMALL, 10, y + 10, "Rate recall:");
    
    // Draw rating options corresponding to buttons
    // Left: Again, Down: Hard, Up: Good, Right: Easy
    int w = renderer.getScreenWidth();
    int col = w / 4;
    
    renderer.drawText(FONT_SMALL, 5, y + 30, "Again");
    renderer.drawText(FONT_SMALL, col + 5, y + 30, "Hard");
    renderer.drawText(FONT_SMALL, 2*col + 5, y + 30, "Good");
    renderer.drawText(FONT_SMALL, 3*col + 5, y + 30, "Easy");
    
    renderer.displayBuffer();
}

void ReviewActivity::showFinished() {
    currentState = FINISHED;
    const bool nothingDue = (reviewedCount == 0 && dueTotal == 0);
    renderer.clearScreen(0xFF);
    renderer.drawCenteredText(FONT_LARGE, renderer.getScreenHeight() / 2 - 20,
                              nothingDue ? "No cards due today" : "Deck Complete!");
    renderer.drawCenteredText(FONT_SMALL, renderer.getScreenHeight() / 2 + 20, "Press Back to exit");
    renderer.displayBuffer();
}

void ReviewActivity::processRating(SM2::Quality quality) {
    // Relearn passes only serve cards already rated this session, so a
    // rating outside a relearn pass is that card's first rating today.
    const bool firstRating = !inRelearnPass;

    // Update card progress
    CardProgress& cp = deckProgress.getCardProgress(currentCard.id);

    // Apply SM-2: ease first, then repetitions, then the interval from the
    // post-update repetition count and the card's previous interval
    cp.ease = SM2::updateEaseFactor(cp.ease, quality);
    if (quality >= SM2::GOOD) {
        cp.repetitions++;
    } else {
        cp.repetitions = 0; // Reset on failure
    }
    cp.interval = SM2::calculateInterval(cp.repetitions, cp.interval, cp.ease);

    if (today >= 0) {
        // Again keeps the card due today (in-session relearning; still due
        // next session if the user exits early). Everything else moves out
        // by the new interval -- Hard lapses to tomorrow and does not repeat.
        cp.due = Relearn::staysDueToday(quality) ? today : today + cp.interval;
        deckProgress.lastReview = today;
    } else {
        cp.due = 0; // no clock: always due
    }

    // Again enqueues for in-session relearning; Hard/Good/Easy dequeue
    relearnQueue.onRated(currentCard.id, quality);

    // Save progress
    // In a real app, we might batch save or save on exit, but safety first
    DeckStorage::saveProgress(deckId, deckProgress);

    if (firstRating) {
        reviewedCount++;
    }

    // Move to the next due card
    if (advanceToNextCard()) {
        showFront();
    } else {
        // Session over: go straight to the summary screen. Remaining = cards
        // still due today that weren't rated this session.
        hasMoreCards = false;
        int remaining = dueTotal - reviewedCount;
        if (remaining < 0) remaining = 0;
        requestNav(NavTarget::SessionComplete, deckId, reviewedCount, remaining);
    }
}

bool ReviewActivity::isCardDue(const String& cardId) const {
    if (today < 0) {
        return true; // no trustworthy clock: review everything
    }
    auto it = deckProgress.cards.find(cardId);
    if (it == deckProgress.cards.end()) {
        return true; // new card, never rated
    }
    return it->second.due <= today;
}

// Re-open the deck stream from the top for another relearn pass
bool ReviewActivity::startRelearnPass() {
    relearnServedThisPass = false;
    cardStream.close();
    return DeckStorage::openCardStream(deckId, cardStream);
}

bool ReviewActivity::advanceToNextCard() {
    if (!inRelearnPass) {
        // Main pass: serve due cards in file order
        while (cardStream.hasNext()) {
            Card card = cardStream.readNext();
            if (!card.id.isEmpty() && isCardDue(card.id)) {
                currentCard = card;
                return true;
            }
        }
        // Main pass exhausted; switch to relearning if anything was rated Again
        if (relearnQueue.empty() || !startRelearnPass()) {
            return false;
        }
        inRelearnPass = true;
    }

    // Relearn passes: serve only queued (Again-rated) cards, re-opening the
    // stream until the queue empties. A pass that serves nothing while the
    // queue is non-empty means the queued IDs vanished from the deck file;
    // bail instead of spinning. The user rating Again forever is fine -- each
    // pass then serves at least one card, so the loop stays user-driven.
    while (true) {
        while (cardStream.hasNext()) {
            Card card = cardStream.readNext();
            if (!card.id.isEmpty() && relearnQueue.contains(card.id)) {
                relearnServedThisPass = true;
                currentCard = card;
                return true;
            }
        }
        if (relearnQueue.empty() || !relearnServedThisPass || !startRelearnPass()) {
            return false;
        }
    }
}
