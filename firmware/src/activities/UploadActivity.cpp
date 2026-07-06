#include "UploadActivity.h"
#include <GfxRenderer.h>
#include <MappedInputManager.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <qrcode.h>

// Heap singleton, deliberately never destroyed: handler lambdas and live
// client connections on the async_tcp task may reference the server after
// the activity that started it has been deleted.
static WebServer& uploadServer() {
    static WebServer* server = new WebServer();
    return *server;
}

UploadActivity::UploadActivity(GfxRenderer& renderer, MappedInputManager& input, bool isX3)
    : Activity("Upload", renderer, input), webServer(uploadServer()), ssid(isX3 ? "Anki-X3" : "Anki-X4") {}

void UploadActivity::onEnter() {
    Activity::onEnter();

    WiFi.mode(WIFI_AP);
    wifiStarted = true;
    apStarted = WiFi.softAP(ssid.c_str(), AP_PASSWORD);

    if (apStarted) {
        Serial.printf("[%lu] Upload: AP \"%s\" up at %s\n", millis(), ssid.c_str(),
                      WiFi.softAPIP().toString().c_str());

        // Best effort: http://anki.local as an alias for 192.168.4.1
        mdnsStarted = MDNS.begin("anki");
        if (mdnsStarted) {
            MDNS.addService("http", "tcp", 80);
        }

        if (!webServer.begin()) {
            Serial.printf("[%lu] Upload: web server failed to start\n", millis());
            stopServices();
        }
    } else {
        Serial.printf("[%lu] Upload: failed to start soft-AP\n", millis());
        stopServices();
    }

    // The server (and its lifetime total) persists across visits; show only
    // decks uploaded during this session
    baseUploadCount = webServer.uploadedCount();
    shownUploadCount = 0;
    drawScreen();
}

void UploadActivity::onExit() {
    Activity::onExit();
    stopServices();
}

void UploadActivity::loop() {
    // Tilt is ignored on this screen; discard gestures so they don't leak
    input.clearTiltEvents();

    if (input.wasPressed(MappedInputManager::Button::Back)) {
        stopServices();
        requestNav(NavTarget::MainMenu);
        return;
    }

    // Poll the upload counter; redraw only when a deck finishes uploading
    if (millis() - lastCountPollMs >= COUNT_POLL_INTERVAL_MS) {
        lastCountPollMs = millis();
        const uint32_t count = webServer.uploadedCount() - baseUploadCount;
        if (count != shownUploadCount) {
            shownUploadCount = count;
            drawScreen();
        }
    }
}

void UploadActivity::stopServices() {
    if (webServer.isRunning()) {
        webServer.end();
    }
    if (mdnsStarted) {
        MDNS.end();
        mdnsStarted = false;
    }
    if (apStarted) {
        WiFi.softAPdisconnect(true);
        // webServer.end() only closes the listening socket; wait for connected
        // clients to drop and give async_tcp time to drain in-flight callbacks
        // (upload chunks writing to SD) before the caller tears anything down.
        const unsigned long start = millis();
        while (WiFi.softAPgetStationNum() > 0 && millis() - start < 1000) {
            delay(10);
        }
        delay(200);
        apStarted = false;
    }
    if (wifiStarted) {
        WiFi.mode(WIFI_OFF);
        wifiStarted = false;
    }
}

void UploadActivity::drawScreen() {
    renderer.clearScreen();

    const int w = renderer.getScreenWidth();
    const int h = renderer.getScreenHeight();

    renderer.fillRect(0, 0, w, HEADER_HEIGHT);
    renderer.drawCenteredText(2, HEADER_HEIGHT / 2 - 10, "Upload Decks", false);

    if (!webServer.isRunning()) {
        renderer.drawCenteredText(1, h / 2 - 15, "Could not start the WiFi hotspot", true);
        renderer.drawCenteredText(1, h / 2 + 15, "Press Back to return", true);
        renderer.displayBuffer();
        return;
    }

    const int textX = 30;
    const int indent = 24;
    const int lineHeight = 34;
    int y = HEADER_HEIGHT + 26;

    renderer.drawText(1, textX, y, "1. Join the WiFi network:", true);
    y += lineHeight;
    renderer.drawText(1, textX + indent, y, ("Network: " + ssid).c_str(), true, EpdFontFamily::BOLD);
    y += lineHeight;
    renderer.drawText(1, textX + indent, y, ("Password: " + String(AP_PASSWORD)).c_str(), true, EpdFontFamily::BOLD);
    y += lineHeight + 12;

    renderer.drawText(1, textX, y, "2. Open in your browser:", true);
    y += lineHeight;
    renderer.drawText(1, textX + indent, y, "http://192.168.4.1", true, EpdFontFamily::BOLD);
    if (mdnsStarted) {
        y += lineHeight;
        renderer.drawText(1, textX + indent, y, "or http://anki.local", true);
    }
    y += lineHeight + 12;

    renderer.drawText(1, textX, y, "3. Upload your .apkg files", true);
    y += lineHeight + 12;

    String countStr = "Decks uploaded: " + String(shownUploadCount);
    renderer.drawText(1, textX, y, countStr.c_str(), true, EpdFontFamily::BOLD);

    renderer.drawCenteredText(1, h - 36, "Press Back when done", true);

    // QR code on the right: scan to join the WiFi network
    const int qrLeft = (w * 3) / 5;
    const int qrRight = w - 20;
    const int qrTop = HEADER_HEIGHT + 20;
    const int qrBottom = h - 80;
    const int maxSize = min(qrRight - qrLeft, qrBottom - qrTop);
    drawWifiQrCode((qrLeft + qrRight) / 2, (qrTop + qrBottom) / 2, maxSize);

    renderer.displayBuffer();
}

void UploadActivity::drawWifiQrCode(int centerX, int centerY, int maxSize) {
    const String payload = "WIFI:T:WPA;S:" + ssid + ";P:" + String(AP_PASSWORD) + ";;";

    // Version 3 (29x29, medium ECC) holds up to 42 bytes; the payload is ~36
    QRCode qr;
    uint8_t modules[qrcode_getBufferSize(3)];
    if (qrcode_initText(&qr, modules, 3, ECC_MEDIUM, payload.c_str()) != 0) {
        return;
    }

    // Leave a 2-module quiet zone on each side (background is already white)
    int scale = maxSize / (qr.size + 4);
    if (scale < 2) scale = 2;
    const int qrPixels = qr.size * scale;
    const int x0 = centerX - qrPixels / 2;
    const int y0 = centerY - qrPixels / 2;

    for (int my = 0; my < qr.size; my++) {
        for (int mx = 0; mx < qr.size; mx++) {
            if (qrcode_getModule(&qr, mx, my)) {
                renderer.fillRect(x0 + mx * scale, y0 + my * scale, scale, scale);
            }
        }
    }

    const char* caption = "Scan to join WiFi";
    const int captionWidth = renderer.getTextWidth(1, caption);
    renderer.drawText(1, centerX - captionWidth / 2, y0 + qrPixels + 12, caption, true);
}
