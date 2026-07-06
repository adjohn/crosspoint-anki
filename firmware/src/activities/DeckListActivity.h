#pragma once

#include "Activity.h"
#include "../storage/DeckStorage.h"
#include <vector>

class DeckListActivity : public Activity {
private:
    // Overlay state machine: List is the normal deck list; Overlay is the
    // per-deck options box; ConfirmDelete is the second-Confirm guard.
    enum class UiState { List, Overlay, ConfirmDelete };

    std::vector<DeckMetadata> decks;
    std::vector<int> dueCounts;  // parallel to decks, from progress.json only
    bool rtcAvailable;
    int selectedIndex;
    int scrollOffset;
    UiState uiState;
    int overlayIndex;
    // millis() at the Confirm press edge while in List state; 0 = no hold in
    // progress. Tracked locally because getHeldTime() is not per-button.
    unsigned long confirmHoldStart;
    String statusMessage;  // brief failure line, cleared on the next list redraw
    static const int ITEMS_PER_PAGE = 10;
    static const int LINE_HEIGHT = 40;
    static const int HEADER_HEIGHT = 50;
    static const int OVERLAY_ITEM_COUNT = 3;
    static constexpr unsigned long CONFIRM_HOLD_MS = 800;

    void reloadDecks();
    void draw();
    void drawListContent();
    void drawOverlayBox();
    void handleListInput();
    void handleOverlayInput();
    void handleConfirmDeleteInput();

public:
    DeckListActivity(GfxRenderer& renderer, MappedInputManager& input, bool rtcAvailable);
    void onEnter() override;
    void loop() override;
};
