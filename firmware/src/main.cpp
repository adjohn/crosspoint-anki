#include <Arduino.h>
#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalGPIO.h>
#include <HalTiltSensor.h>
#include <Preferences.h>
#include <SDCardManager.h>
#include <builtinFonts/all.h>

#include "MappedInputManager.h"
#include "activities/DeckListActivity.h"
#include "activities/MainMenuActivity.h"
#include "activities/ReviewActivity.h"
#include "activities/SessionCompleteActivity.h"
#include "activities/UploadActivity.h"
#include "utils/BootUtils.h"
#include "utils/TimeUtils.h"
#include "fontIds.h"

// Hardware
HalDisplay display;
HalGPIO gpio;
HalTiltSensor tiltSensor;
MappedInputManager mappedInputManager(gpio);
GfxRenderer renderer(display);

// Current activity
Activity* currentActivity = nullptr;

// Fonts - using a minimal set for the app
EpdFont bookerly14RegularFont(&bookerly_14_regular);
EpdFont bookerly14BoldFont(&bookerly_14_bold);
EpdFont bookerly14ItalicFont(&bookerly_14_italic);
EpdFont bookerly14BoldItalicFont(&bookerly_14_bolditalic);
EpdFontFamily bookerly14FontFamily(&bookerly14RegularFont, &bookerly14BoldFont, 
                                   &bookerly14ItalicFont, &bookerly14BoldItalicFont);

EpdFont ui12RegularFont(&ubuntu_12_regular);
EpdFont ui12BoldFont(&ubuntu_12_bold);
EpdFontFamily ui12FontFamily(&ui12RegularFont, &ui12BoldFont);

EpdFont smallFont(&notosans_8_regular);
EpdFontFamily smallFontFamily(&smallFont);

// Power button hold duration for sleep (ms)
static constexpr unsigned long POWER_BUTTON_DURATION = 1000;
// Back button hold duration for exit to CrossPoint (ms)
static constexpr unsigned long BACK_BUTTON_EXIT_DURATION = 1200;
// Auto-sleep timeout (ms) - 5 minutes
static constexpr unsigned long SLEEP_TIMEOUT_MS = 5 * 60 * 1000;

void exitActivity() {
    if (currentActivity) {
        currentActivity->onExit();
        delete currentActivity;
        currentActivity = nullptr;
    }
}

void enterNewActivity(Activity* activity) {
    // Drop tilt gestures queued during the previous activity
    mappedInputManager.clearTiltEvents();
    currentActivity = activity;
    currentActivity->onEnter();
}

void enterDeepSleep() {
    Serial.printf("[%lu] Entering deep sleep...\n", millis());

    // Tear down the current activity (stops soft-AP/mDNS/web server and
    // cleans up in-flight uploads) before powering anything down
    exitActivity();

    // Clear screen before sleep
    renderer.clearScreen();
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, "Sleeping...", true);
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);

    tiltSensor.deepSleep();
    display.deepSleep();
    gpio.startDeepSleep();
}

void exitToCrossPoint() {
    Serial.printf("[%lu] Returning to CrossPoint...\n", millis());

    renderer.clearScreen();
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2, "Returning to CrossPoint...", true);
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);

    CrossPoint::returnToCrossPoint();
}

void handleNavRequest(const Activity::NavRequest& nav) {
    switch (nav.target) {
        case Activity::NavTarget::MainMenu:
            exitActivity();
            enterNewActivity(new MainMenuActivity(renderer, mappedInputManager));
            break;
        case Activity::NavTarget::DeckList:
            exitActivity();
            enterNewActivity(new DeckListActivity(renderer, mappedInputManager, gpio.deviceIsX3()));
            break;
        case Activity::NavTarget::Review:
            exitActivity();
            enterNewActivity(new ReviewActivity(nav.deckId, renderer, mappedInputManager, gpio.deviceIsX3()));
            break;
        case Activity::NavTarget::SessionComplete:
            exitActivity();
            enterNewActivity(new SessionCompleteActivity(renderer, mappedInputManager, nav.reviewed, nav.remaining));
            break;
        case Activity::NavTarget::Upload:
            exitActivity();
            enterNewActivity(new UploadActivity(renderer, mappedInputManager, gpio.deviceIsX3()));
            break;
        case Activity::NavTarget::ExitApp:
            exitActivity();
            exitToCrossPoint();
            break;
        case Activity::NavTarget::None:
            break;
    }
}

void showError(const char* message) {
    renderer.clearScreen();
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2 - 20, "Error", true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2 + 20, message, true);
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}

void setup() {
    // Initialize GPIO first (runs X4/X3 device detection)
    gpio.begin();
    const bool isX3 = gpio.deviceIsX3();

    // Start serial if USB connected
    Serial.begin(115200);
    unsigned long start = millis();
    while (!Serial && (millis() - start) < 2000) {
        delay(10);
    }

    Serial.printf("[%lu] Flashink %s Starting (version %s)\n", millis(), isX3 ? "X3" : "X4", FLASHINK_VERSION);

    // Initialize SD card
    if (!SdMan.begin()) {
        Serial.printf("[%lu] SD card initialization failed!\n", millis());
        display.begin(isX3);
        renderer.insertFont(UI_12_FONT_ID, ui12FontFamily);
        showError("SD card error");
        delay(3000);
        CrossPoint::returnToCrossPoint();
        return;
    }
    Serial.printf("[%lu] SD card initialized\n", millis());

    // Initialize display
    display.begin(isX3);
    Serial.printf("[%lu] Display initialized\n", millis());

    // Tilt input (X3 only; no-op on X4)
    tiltSensor.begin(gpio);
    mappedInputManager.setTiltSensor(&tiltSensor);
    if (tiltSensor.isAvailable()) {
        Preferences prefs;
        prefs.begin("flashink", true);
        const bool tiltOn = prefs.getBool("tilt", true);
        prefs.end();
        mappedInputManager.setTiltEnabled(tiltOn);
        tiltSensor.update(tiltOn);  // Kick the wake/sleep state machine so the gyro comes up enabled
        Serial.printf("[%lu] Tilt input %s\n", millis(), tiltOn ? "enabled" : "disabled");
    }

    // Timezone offset for the local day boundary (persisted from the main
    // menu or a deck upload); must be set before any activity computes "today"
    {
        Preferences prefs;
        prefs.begin("flashink", true);
        TimeUtils::setTimezoneOffsetMinutes(prefs.getInt("tzmin", 0));
        prefs.end();
        Serial.printf("[%lu] Day cutoff offset: %d minutes\n", millis(), TimeUtils::timezoneOffsetMinutes());
    }

    // Setup fonts
    renderer.insertFont(BOOKERLY_14_FONT_ID, bookerly14FontFamily);
    renderer.insertFont(UI_12_FONT_ID, ui12FontFamily);
    renderer.insertFont(SMALL_FONT_ID, smallFontFamily);
    // Activities draw with small literal font IDs; alias them to real families
    renderer.insertFont(1, ui12FontFamily);
    renderer.insertFont(2, bookerly14FontFamily);
    renderer.insertFont(4, bookerly14FontFamily);
    Serial.printf("[%lu] Fonts loaded\n", millis());
    
    // Show boot screen
    renderer.clearScreen();
    renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2 - 20, isX3 ? "Flashink X3" : "Flashink X4", true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() / 2 + 20, FLASHINK_VERSION, true);
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    
    delay(500);
    
    // Enter main menu
    exitActivity();
    enterNewActivity(new MainMenuActivity(renderer, mappedInputManager));
    
    Serial.printf("[%lu] Setup complete, entering main loop\n", millis());
}

void loop() {
    static unsigned long lastActivityTime = millis();
    static unsigned long lastMemPrint = 0;
    
    // Update button states
    gpio.update();

    // Poll tilt gestures (no-op on X4; sleeps the gyro while tilt is toggled off)
    tiltSensor.update(mappedInputManager.isTiltEnabled());

    // Memory logging (every 10 seconds)
    if (Serial && millis() - lastMemPrint >= 10000) {
        Serial.printf("[%lu] [MEM] Free: %d bytes, Min Free: %d bytes\n", 
                      millis(), ESP.getFreeHeap(), ESP.getMinFreeHeap());
        lastMemPrint = millis();
    }
    
    // Check for user activity; keep-awake activities (e.g. the upload
    // server) also reset the timer so the AP isn't killed mid-upload
    if (gpio.wasAnyPressed() || gpio.wasAnyReleased() || tiltSensor.hadActivity() ||
        (currentActivity && currentActivity->keepAwake())) {
        lastActivityTime = millis();
    }

    // Auto-sleep after timeout
    if (millis() - lastActivityTime >= SLEEP_TIMEOUT_MS) {
        Serial.printf("[%lu] Auto-sleep triggered\n", millis());
        enterDeepSleep();
        return;
    }
    
    // Power button long press -> sleep
    if (gpio.isPressed(HalGPIO::BTN_POWER) && gpio.getHeldTime() > POWER_BUTTON_DURATION) {
        enterDeepSleep();
        return;
    }

    // Back button long press -> exit to CrossPoint. The hold is tracked from
    // Back's own press edge (getHeldTime() is not per-button: it measures from
    // the first button pressed while any button is down, so a Back tap during
    // another button's hold would misfire). The short-press edge is still
    // consumed by the activity when the hold begins, and returnToCrossPoint()
    // restarts so nothing else runs after this fires.
    static unsigned long backHoldStart = 0;
    if (mappedInputManager.isPressed(MappedInputManager::Button::Back)) {
        if (mappedInputManager.wasPressed(MappedInputManager::Button::Back)) {
            backHoldStart = millis();
        }
        if (backHoldStart != 0 && millis() - backHoldStart >= BACK_BUTTON_EXIT_DURATION) {
            exitActivity();
            exitToCrossPoint();
            return;
        }
    } else {
        backHoldStart = 0;
    }

    // Run current activity, then apply any navigation it requested
    if (currentActivity) {
        currentActivity->loop();
        const Activity::NavRequest nav = currentActivity->consumeNavRequest();
        if (nav.target != Activity::NavTarget::None) {
            handleNavRequest(nav);
        }
    }

    // Small delay to prevent tight spinning
    delay(10);
}
