#include "DeckListActivity.h"
#include "../utils/TimeUtils.h"
#include <GfxRenderer.h>
#include <MappedInputManager.h>
#include <algorithm>

namespace {
const char* const OVERLAY_ITEMS[] = {"Reset progress", "Delete deck", "Cancel"};
}

DeckListActivity::DeckListActivity(GfxRenderer& renderer, MappedInputManager& input, bool rtcAvailable)
    : Activity("DeckList", renderer, input), rtcAvailable(rtcAvailable), selectedIndex(0), scrollOffset(0),
      uiState(UiState::List), overlayIndex(0), confirmHoldStart(0) {}

void DeckListActivity::onEnter() {
    Activity::onEnter();
    reloadDecks();
    draw();
}

void DeckListActivity::reloadDecks() {
    decks = DeckStorage::listDecks();

    std::sort(decks.begin(), decks.end(), [](const DeckMetadata& a, const DeckMetadata& b) {
        return a.name < b.name;
    });

    // Due counts come from progress.json alone (no cards.jsonl streaming).
    // With no trustworthy clock (today == -1) every card counts as due.
    const int32_t today = TimeUtils::todayEpochDays(rtcAvailable);
    dueCounts.clear();
    dueCounts.reserve(decks.size());
    for (const auto& deck : decks) {
        DeckProgress progress = DeckStorage::loadProgress(deck.id);
        dueCounts.push_back(progress.countDue(today, deck.cardCount));
    }

    // Clamp selection and scroll after the list shrinks (e.g. a deck was deleted)
    if (selectedIndex > (int)decks.size() - 1) {
        selectedIndex = (int)decks.size() - 1;
    }
    if (selectedIndex < 0) {
        selectedIndex = 0;
    }
    int maxScroll = (int)decks.size() - ITEMS_PER_PAGE;
    if (maxScroll < 0) {
        maxScroll = 0;
    }
    if (scrollOffset > maxScroll) {
        scrollOffset = maxScroll;
    }
    if (scrollOffset > selectedIndex) {
        scrollOffset = selectedIndex;
    }
}

void DeckListActivity::loop() {
    if (uiState != UiState::List) {
        // Tilt gestures must not act on the overlay
        input.clearTiltEvents();
        if (uiState == UiState::Overlay) {
            handleOverlayInput();
        } else {
            handleConfirmDeleteInput();
        }
        return;
    }
    handleListInput();
}

void DeckListActivity::handleListInput() {
    // Tilt gestures are consume-on-read: read once per frame
    const bool tiltForward = input.wasTiltedForward();
    const bool tiltBack = input.wasTiltedBack();

    // A Confirm hold is in progress: short release opens the deck, holding to
    // the threshold opens the options overlay. The release after a long hold
    // never reaches List handling because the overlay reacts to press edges
    // only, so it is swallowed naturally. Other input is ignored meanwhile.
    if (confirmHoldStart != 0) {
        if (!input.isPressed(MappedInputManager::Button::Confirm)) {
            confirmHoldStart = 0;
            requestNav(NavTarget::Review, decks[selectedIndex].id);
        } else if (millis() - confirmHoldStart >= CONFIRM_HOLD_MS) {
            confirmHoldStart = 0;
            uiState = UiState::Overlay;
            overlayIndex = 0;
            statusMessage = "";
            draw();
        }
        return;
    }

    bool needsRedraw = false;

    if (input.wasPressed(MappedInputManager::Button::Down) || tiltForward) {
        if (selectedIndex < (int)decks.size() - 1) {
            selectedIndex++;
            if (selectedIndex >= scrollOffset + ITEMS_PER_PAGE) {
                scrollOffset = selectedIndex - ITEMS_PER_PAGE + 1;
            }
            needsRedraw = true;
        }
    } else if (input.wasPressed(MappedInputManager::Button::Up) || tiltBack) {
        if (selectedIndex > 0) {
            selectedIndex--;
            if (selectedIndex < scrollOffset) {
                scrollOffset = selectedIndex;
            }
            needsRedraw = true;
        }
    } else if (input.wasPressed(MappedInputManager::Button::Confirm)) {
        if (!decks.empty()) {
            confirmHoldStart = millis();
        }
    } else if (input.wasPressed(MappedInputManager::Button::Back)) {
        requestNav(NavTarget::MainMenu);
    }

    if (needsRedraw) {
        statusMessage = "";
        draw();
    }
}

void DeckListActivity::handleOverlayInput() {
    if (input.wasPressed(MappedInputManager::Button::Down)) {
        overlayIndex = (overlayIndex + 1) % OVERLAY_ITEM_COUNT;
        draw();
    } else if (input.wasPressed(MappedInputManager::Button::Up)) {
        overlayIndex = (overlayIndex + OVERLAY_ITEM_COUNT - 1) % OVERLAY_ITEM_COUNT;
        draw();
    } else if (input.wasPressed(MappedInputManager::Button::Confirm)) {
        if (overlayIndex == 0) {
            // Reset progress
            if (!DeckStorage::resetProgress(decks[selectedIndex].id)) {
                statusMessage = "Reset progress failed";
            }
            uiState = UiState::List;
            reloadDecks();
            draw();
        } else if (overlayIndex == 1) {
            uiState = UiState::ConfirmDelete;
            draw();
        } else {
            // Cancel
            uiState = UiState::List;
            draw();
        }
    } else if (input.wasPressed(MappedInputManager::Button::Back)) {
        uiState = UiState::List;
        draw();
    }
}

void DeckListActivity::handleConfirmDeleteInput() {
    if (input.wasPressed(MappedInputManager::Button::Confirm)) {
        if (!DeckStorage::deleteDeck(decks[selectedIndex].id)) {
            statusMessage = "Delete failed";
        }
        uiState = UiState::List;
        reloadDecks();
        draw();
    } else if (input.wasPressed(MappedInputManager::Button::Back) ||
               input.wasPressed(MappedInputManager::Button::Up) ||
               input.wasPressed(MappedInputManager::Button::Down) ||
               input.wasPressed(MappedInputManager::Button::Left) ||
               input.wasPressed(MappedInputManager::Button::Right)) {
        // Any button other than Confirm cancels the delete
        uiState = UiState::List;
        draw();
    }
}

void DeckListActivity::draw() {
    renderer.clearScreen();
    drawListContent();
    if (uiState != UiState::List) {
        drawOverlayBox();
    }
    renderer.displayBuffer();
}

void DeckListActivity::drawListContent() {
    renderer.fillRect(0, 0, renderer.getScreenWidth(), HEADER_HEIGHT);
    renderer.drawCenteredText(2, HEADER_HEIGHT / 2 - 10, "Select Deck", false);

    int y = HEADER_HEIGHT + 10;

    if (decks.empty()) {
        renderer.drawCenteredText(1, 200, "No decks found on SD card");
    } else {
        for (int i = 0; i < ITEMS_PER_PAGE; i++) {
            int deckIndex = scrollOffset + i;
            if (deckIndex >= (int)decks.size()) break;

            const auto& deck = decks[deckIndex];
            bool isSelected = (deckIndex == selectedIndex);

            String countStr = String(dueCounts[deckIndex]) + " due";
            int countWidth = renderer.getTextWidth(1, countStr.c_str());

            if (isSelected) {
                renderer.fillRect(0, y - 5, renderer.getScreenWidth(), LINE_HEIGHT);
                renderer.drawText(1, 10, y, deck.name.c_str(), false);
                renderer.drawText(1, renderer.getScreenWidth() - countWidth - 10, y, countStr.c_str(), false);
            } else {
                renderer.drawText(1, 10, y, deck.name.c_str(), true);
                renderer.drawText(1, renderer.getScreenWidth() - countWidth - 10, y, countStr.c_str(), true);
            }

            y += LINE_HEIGHT;
        }
    }

    if (!statusMessage.isEmpty()) {
        // White out a strip first: a full page of rows can reach this far down
        renderer.fillRect(0, renderer.getScreenHeight() - 42, renderer.getScreenWidth(), 42, false);
        renderer.drawCenteredText(1, renderer.getScreenHeight() - 36, statusMessage.c_str());
    }

    if (decks.size() > ITEMS_PER_PAGE) {
        int scrollBarHeight = (ITEMS_PER_PAGE * renderer.getScreenHeight()) / decks.size();
        int scrollBarY = HEADER_HEIGHT + (scrollOffset * (renderer.getScreenHeight() - HEADER_HEIGHT)) / decks.size();
        renderer.fillRect(renderer.getScreenWidth() - 4, scrollBarY, 4, scrollBarHeight);
    }
}

void DeckListActivity::drawOverlayBox() {
    const int screenW = renderer.getScreenWidth();
    const int screenH = renderer.getScreenHeight();
    const int boxW = 400;
    const int titleH = 44;
    const int itemH = LINE_HEIGHT;
    const int padding = 14;
    const int boxH = titleH + OVERLAY_ITEM_COUNT * itemH + 2 * padding;
    const int boxX = (screenW - boxW) / 2;
    const int boxY = (screenH - boxH) / 2;

    // White out the box area, then a 2px border
    renderer.fillRect(boxX, boxY, boxW, boxH, false);
    renderer.drawRect(boxX, boxY, boxW, boxH, 2, true);

    // Title: the deck name, truncated to fit
    const std::string title = renderer.truncatedText(1, decks[selectedIndex].name.c_str(), boxW - 2 * padding);
    renderer.drawText(1, boxX + padding, boxY + padding, title.c_str(), true);
    renderer.drawLine(boxX, boxY + titleH, boxX + boxW - 1, boxY + titleH, true);

    if (uiState == UiState::ConfirmDelete) {
        const std::string name =
            renderer.truncatedText(1, ("'" + decks[selectedIndex].name + "'").c_str(), boxW - 2 * padding);
        int y = boxY + titleH + padding;
        renderer.drawCenteredText(1, y, "Press Confirm again to delete");
        renderer.drawCenteredText(1, y + itemH, name.c_str());
        renderer.drawCenteredText(1, y + 2 * itemH, "Any other button cancels");
        return;
    }

    int y = boxY + titleH + padding;
    for (int i = 0; i < OVERLAY_ITEM_COUNT; i++) {
        if (i == overlayIndex) {
            renderer.fillRect(boxX + 2, y - 5, boxW - 4, itemH, true);
            renderer.drawText(1, boxX + padding, y, OVERLAY_ITEMS[i], false);
        } else {
            renderer.drawText(1, boxX + padding, y, OVERLAY_ITEMS[i], true);
        }
        y += itemH;
    }
}
