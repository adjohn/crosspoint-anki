#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <SDCardManager.h>

// Static files directory on SD card
#define WEB_ROOT "/.crosspoint/apps/anki/web"
#define UPLOAD_TEMP_DIR "/.crosspoint/apps/anki/temp"

// Upload limits
#define MAX_UPLOAD_SIZE (10 * 1024 * 1024)  // 10MB
#define UPLOAD_BUFFER_SIZE 1024              // 1KB chunks for streaming

class WebServer {
public:
    WebServer(uint16_t port = 80);
    
    // Initialize and start the web server
    bool begin();
    
    // Stop the web server
    void end();
    
    // Check if server is running
    bool isRunning() const { return running; }

    // Number of decks successfully uploaded since construction.
    // Incremented from the async upload callback; volatile is enough for a
    // single 32-bit counter polled from the main loop.
    uint32_t uploadedCount() const { return uploadCount; }

private:
    AsyncWebServer server;
    bool running = false;
    bool routesConfigured = false;
    volatile uint32_t uploadCount = 0;

    // Per-upload state; uploads are serialized, activeUpload owns this state
    // and concurrent uploads are rejected with 409.
    AsyncWebServerRequest* activeUpload = nullptr;
    FsFile uploadFile;
    size_t uploadTotalBytes = 0;
    size_t uploadLineCount = 0;
    uint8_t uploadLastByte = '\n';
    String uploadTempPath;
    bool uploadHasError = false;

    // Setup routes
    void setupRoutes();

    // Send a JSON response and mark the request as answered (via _tempObject,
    // freed by the request destructor) so the POST handler can detect uploads
    // that never produced a response (e.g. zero-byte file parts).
    static void sendJson(AsyncWebServerRequest* request, int code, const String& body);
    
    // Static file handler - serves files from SD card
    void handleStaticFile(AsyncWebServerRequest* request);
    
    // Upload handler - streams JSONL data directly to SD card
    void handleUploadDeck(AsyncWebServerRequest* request, String filename, size_t index, 
                          uint8_t* data, size_t len, bool final);
    
    // Helper to get content type from file extension
    String getContentType(const String& filename);
    
    // Helper to get MIME type for web files
    String getMimeType(const String& path);
};
