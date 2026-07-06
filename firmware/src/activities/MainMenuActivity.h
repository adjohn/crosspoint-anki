#pragma once

#include "Activity.h"
#include <vector>

class MainMenuActivity : public Activity {
private:
    std::vector<String> menuItems;
    int selectedIndex;
    int tiltItemIndex;  // -1 when the tilt toggle is hidden (non-X3)
    int exitItemIndex;
    static const int ITEM_HEIGHT = 40;
    static const int HEADER_HEIGHT = 60;

    const char* tiltLabel() const;
    void toggleTilt();
    void drawMenu();

public:
    MainMenuActivity(GfxRenderer& renderer, MappedInputManager& input);
    void onEnter() override;
    void loop() override;
};
