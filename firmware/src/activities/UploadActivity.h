#pragma once

#include "Activity.h"
#include "../network/WebServer.h"

class UploadActivity : public Activity {
private:
    // Reference to a never-destroyed singleton: async_tcp callbacks (in-flight
    // uploads, client disconnects) can still touch the server after this
    // activity is deleted, so the server object must outlive it.
    WebServer& webServer;
    String ssid;
    bool wifiStarted = false;
    bool apStarted = false;
    bool mdnsStarted = false;
    uint32_t baseUploadCount = 0;
    uint32_t shownUploadCount = 0;
    unsigned long lastCountPollMs = 0;

    static const int HEADER_HEIGHT = 60;
    static constexpr const char* AP_PASSWORD = "flashink123";
    static const unsigned long COUNT_POLL_INTERVAL_MS = 500;

    void stopServices();
    void drawScreen();
    void drawWifiQrCode(int x, int y, int maxSize);

public:
    UploadActivity(GfxRenderer& renderer, MappedInputManager& input, bool isX3);
    void onEnter() override;
    void onExit() override;
    void loop() override;
    bool keepAwake() const override { return webServer.isRunning(); }
};
