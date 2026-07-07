#include "MainMenuActivity.h"
#include <GfxRenderer.h>
#include <MappedInputManager.h>
#include <Preferences.h>
#include <cstdio>
#include "../utils/TimeUtils.h"

MainMenuActivity::MainMenuActivity(GfxRenderer& renderer, MappedInputManager& input)
    : Activity("MainMenu", renderer, input), selectedIndex(0), tiltItemIndex(-1) {
    menuItems.push_back("Study");
    uploadItemIndex = (int)menuItems.size();
    menuItems.push_back("Upload Decks");
    if (input.tiltAvailable()) {
        // X3 only: the X4 has no tilt sensor
        tiltItemIndex = (int)menuItems.size();
        menuItems.push_back(tiltLabel());
    }
    tzItemIndex = (int)menuItems.size();
    menuItems.push_back(tzLabel());
    exitItemIndex = (int)menuItems.size();
    menuItems.push_back("Exit to CrossPoint");
}

const char* MainMenuActivity::tiltLabel() const {
    return input.isTiltEnabled() ? "Tilt: On" : "Tilt: Off";
}

void MainMenuActivity::toggleTilt() {
    input.setTiltEnabled(!input.isTiltEnabled());

    Preferences prefs;
    prefs.begin("flashink", false);
    prefs.putBool("tilt", input.isTiltEnabled());
    prefs.end();

    menuItems[tiltItemIndex] = tiltLabel();
}

// "Day cutoff: UTC+H[:MM]" — minutes shown only for non-whole-hour offsets
// (UTC+0, UTC-3:30, UTC+5:30)
String MainMenuActivity::tzLabel() const {
    const int minutes = TimeUtils::timezoneOffsetMinutes();
    const int abs = minutes < 0 ? -minutes : minutes;
    char buf[32];
    if (abs % 60 == 0) {
        snprintf(buf, sizeof(buf), "Day cutoff: UTC%c%d", minutes < 0 ? '-' : '+', abs / 60);
    } else {
        snprintf(buf, sizeof(buf), "Day cutoff: UTC%c%d:%02d", minutes < 0 ? '-' : '+', abs / 60, abs % 60);
    }
    return String(buf);
}

void MainMenuActivity::adjustTimezone(int deltaMinutes) {
    // setTimezoneOffsetMinutes clamps to [-720, 840]; no wrap-around
    TimeUtils::setTimezoneOffsetMinutes(TimeUtils::timezoneOffsetMinutes() + deltaMinutes);

    Preferences prefs;
    prefs.begin("flashink", false);
    prefs.putInt("tzmin", TimeUtils::timezoneOffsetMinutes());
    prefs.end();

    menuItems[tzItemIndex] = tzLabel();
}

void MainMenuActivity::onEnter() {
    Activity::onEnter();
    drawMenu();
}

void MainMenuActivity::loop() {
    bool needsRedraw = false;

    // Tilt gestures are consume-on-read: read once per frame
    const bool tiltForward = input.wasTiltedForward();
    const bool tiltBack = input.wasTiltedBack();

    if (input.wasPressed(MappedInputManager::Button::Down) || tiltForward) {
        if (selectedIndex < (int)menuItems.size() - 1) {
            selectedIndex++;
            needsRedraw = true;
        }
    } else if (input.wasPressed(MappedInputManager::Button::Up) || tiltBack) {
        if (selectedIndex > 0) {
            selectedIndex--;
            needsRedraw = true;
        }
    } else if (input.wasPressed(MappedInputManager::Button::Confirm)) {
        if (selectedIndex == 0) {
            requestNav(NavTarget::DeckList);
        } else if (selectedIndex == uploadItemIndex) {
            requestNav(NavTarget::Upload);
        } else if (selectedIndex == tiltItemIndex) {
            toggleTilt();
            needsRedraw = true;
        } else if (selectedIndex == exitItemIndex) {
            requestNav(NavTarget::ExitApp);
        }
        // Confirm on the day-cutoff item intentionally does nothing:
        // Left/Right are the adjustment inputs
    } else if (selectedIndex == tzItemIndex &&
               (input.wasPressed(MappedInputManager::Button::Left) ||
                input.wasPressed(MappedInputManager::Button::Right))) {
        adjustTimezone(input.wasPressed(MappedInputManager::Button::Right) ? 30 : -30);
        needsRedraw = true;
    }

    if (needsRedraw) {
        drawMenu();
    }
}

void MainMenuActivity::drawMenu() {
    renderer.clearScreen();

    renderer.fillRect(0, 0, renderer.getScreenWidth(), HEADER_HEIGHT);
    renderer.drawCenteredText(2, HEADER_HEIGHT / 2 - 10, "Flashink", false);

    int y = HEADER_HEIGHT + 20;

    for (int i = 0; i < (int)menuItems.size(); i++) {
        bool isSelected = (i == selectedIndex);

        if (isSelected) {
            renderer.fillRect(0, y - 5, renderer.getScreenWidth(), ITEM_HEIGHT);
            renderer.drawCenteredText(1, y, menuItems[i].c_str(), false);
        } else {
            renderer.drawCenteredText(1, y, menuItems[i].c_str(), true);
        }

        y += ITEM_HEIGHT;
    }

    renderer.displayBuffer();
}
