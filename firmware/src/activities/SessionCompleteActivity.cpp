#include "SessionCompleteActivity.h"
#include <GfxRenderer.h>
#include <MappedInputManager.h>

SessionCompleteActivity::SessionCompleteActivity(GfxRenderer& renderer, MappedInputManager& input, int reviewed, int remaining)
    : Activity("SessionComplete", renderer, input), cardsReviewed(reviewed), cardsRemaining(remaining) {}

void SessionCompleteActivity::onEnter() {
    Activity::onEnter();
    drawScreen();
}

void SessionCompleteActivity::loop() {
    // Tilt is intentionally ignored here; discard gestures so an accidental
    // flick neither acts on this screen nor leaks into the next activity.
    input.clearTiltEvents();

    if (input.wasPressed(MappedInputManager::Button::Confirm) || input.wasPressed(MappedInputManager::Button::Back)) {
        requestNav(NavTarget::DeckList);
    }
}

void SessionCompleteActivity::drawScreen() {
    renderer.clearScreen();
    
    int centerX = renderer.getScreenWidth() / 2;
    int centerY = renderer.getScreenHeight() / 2;
    
    renderer.drawCenteredText(2, centerY - 60, "Session Complete!", true);

    String reviewedStr = "Reviewed: " + String(cardsReviewed);
    renderer.drawCenteredText(1, centerY - 10, reviewedStr.c_str(), true);

    String remainingStr = "Remaining: " + String(cardsRemaining);
    renderer.drawCenteredText(1, centerY + 20, remainingStr.c_str(), true);

    renderer.fillRect(40, centerY + 60, renderer.getScreenWidth() - 80, 40);
    renderer.drawCenteredText(1, centerY + 70, "Back to Decks", false); // white text on the black bar

    renderer.displayBuffer();
}
