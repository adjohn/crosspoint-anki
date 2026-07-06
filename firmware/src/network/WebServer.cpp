#include "WebServer.h"
#include <WiFi.h>
#include <memory>
#include "../data/DeckMetadata.h"
#include "../storage/DeckStorage.h"

WebServer::WebServer(uint16_t port) : server(port) {}

bool WebServer::begin() {
    if (running) {
        return true;
    }
    
    if (!SdMan.ready()) {
        Serial.println("WebServer: SD card not ready");
        return false;
    }
    
    SdMan.ensureDirectoryExists(UPLOAD_TEMP_DIR);
    SdMan.ensureDirectoryExists(DeckStorage::getDecksDir().c_str());

    if (!routesConfigured) {
        setupRoutes();  // server.on() appends; only register handlers once
        routesConfigured = true;
    }
    server.begin();
    running = true;
    
    Serial.println("WebServer: Started on port " + String(80));
    return true;
}

void WebServer::end() {
    if (running) {
        server.end();
        running = false;
        Serial.println("WebServer: Stopped");
    }
}

void WebServer::setupRoutes() {
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request->redirect("/upload.html");
    });
    
    server.on("/upload-deck", HTTP_POST,
        [](AsyncWebServerRequest* request) {
            // The library never invokes the upload callback for a zero-byte or
            // missing file part, so nothing responded yet - fail explicitly
            // instead of leaving the client to hang until timeout.
            if (request->_tempObject == nullptr) {
                request->send(400, "application/json", "{\"error\":\"Empty or missing file\"}");
            }
        },
        [this](AsyncWebServerRequest* request, String filename, size_t index,
               uint8_t* data, size_t len, bool final) {
            handleUploadDeck(request, filename, index, data, len, final);
        }
    );
    
    server.onNotFound([this](AsyncWebServerRequest* request) {
        handleStaticFile(request);
    });
}

void WebServer::handleStaticFile(AsyncWebServerRequest* request) {
    String path = request->url();

    if (path.endsWith("/")) {
        path += "index.html";
    }

    if (path.indexOf("..") != -1 || path.indexOf("//") != -1) {
        request->send(403, "text/plain", "Forbidden");
        return;
    }

    String fullPath = String(WEB_ROOT) + path;

    if (!SdMan.exists(fullPath.c_str())) {
        request->send(404, "text/plain", "File not found");
        return;
    }

    // shared_ptr with a closing deleter: the copy captured in the fill lambda
    // lives inside the response, so the file is closed and freed when the
    // response is destroyed - including client disconnects mid-transfer.
    std::shared_ptr<FsFile> file(new FsFile(SdMan.open(fullPath.c_str(), O_RDONLY)),
                                 [](FsFile* f) {
                                     if (f) {
                                         if (*f) f->close();
                                         delete f;
                                     }
                                 });
    if (!(*file) || file->isDirectory()) {
        request->send(404, "text/plain", "File not found");
        return;
    }

    size_t fileSize = file->size();
    String contentType = getMimeType(path);

    AsyncWebServerResponse* response = request->beginResponse(
        contentType,
        fileSize,
        [file](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
            if (!(*file)) return 0;
            int bytesRead = file->read(buffer, maxLen);
            if (bytesRead <= 0) {
                file->close();
                return 0;
            }
            if (!file->available()) {
                file->close();
            }
            return (size_t)bytesRead;
        }
    );

    response->addHeader("Cache-Control", "public, max-age=3600");
    request->send(response);
}

void WebServer::sendJson(AsyncWebServerRequest* request, int code, const String& body) {
    if (request->_tempObject == nullptr) {
        request->_tempObject = malloc(1);  // "responded" marker, freed by the request
    }
    request->send(code, "application/json", body);
}

void WebServer::handleUploadDeck(AsyncWebServerRequest* request, String filename, size_t index,
                                  uint8_t* data, size_t len, bool final) {
    if (index == 0) {
        if (activeUpload != nullptr && activeUpload != request) {
            sendJson(request, 409, "{\"error\":\"Another upload is in progress\"}");
            return;
        }
        activeUpload = request;
        uploadTotalBytes = 0;
        uploadLineCount = 0;
        uploadLastByte = '\n';
        uploadHasError = false;
        uploadFile.close();

        // Release the upload slot (and any partial temp file) if the client
        // drops mid-upload or after an error.
        request->onDisconnect([this, request]() {
            if (activeUpload == request) {
                if (uploadFile) {
                    uploadFile.close();
                    SdMan.remove(uploadTempPath.c_str());
                }
                activeUpload = nullptr;
            }
        });

        if (!request->hasParam("deckId", true)) {
            sendJson(request, 400, "{\"error\":\"Missing deckId parameter\"}");
            uploadHasError = true;
            return;
        }

        String deckId = request->getParam("deckId", true)->value();
        if (deckId.length() == 0 || deckId.length() > 64) {
            sendJson(request, 400, "{\"error\":\"Invalid deckId\"}");
            uploadHasError = true;
            return;
        }

        for (size_t i = 0; i < deckId.length(); i++) {
            char c = deckId[i];
            if (!isalnum(c) && c != '-' && c != '_') {
                sendJson(request, 400, "{\"error\":\"Invalid deckId characters\"}");
                uploadHasError = true;
                return;
            }
        }

        SdMan.ensureDirectoryExists(UPLOAD_TEMP_DIR);
        uploadTempPath = String(UPLOAD_TEMP_DIR) + "/" + deckId + ".jsonl.tmp";

        if (SdMan.exists(uploadTempPath.c_str())) {
            SdMan.remove(uploadTempPath.c_str());
        }

        uploadFile = SdMan.open(uploadTempPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
        if (!uploadFile) {
            sendJson(request, 500, "{\"error\":\"Failed to create upload file\"}");
            uploadHasError = true;
            return;
        }

        Serial.println("WebServer: Starting upload for deck: " + deckId);
    }

    if (request != activeUpload || uploadHasError) {
        return;
    }

    if (len > 0 && uploadFile) {
        uploadTotalBytes += len;

        if (uploadTotalBytes > MAX_UPLOAD_SIZE) {
            uploadFile.close();
            SdMan.remove(uploadTempPath.c_str());
            sendJson(request, 413, "{\"error\":\"File too large (max 10MB)\"}");
            uploadHasError = true;
            return;
        }

        for (size_t i = 0; i < len; i++) {
            if (data[i] == '\n') uploadLineCount++;
        }
        uploadLastByte = data[len - 1];

        size_t written = uploadFile.write(data, len);
        if (written != len) {
            uploadFile.close();
            SdMan.remove(uploadTempPath.c_str());
            sendJson(request, 500, "{\"error\":\"Write failed\"}");
            uploadHasError = true;
            return;
        }
    }

    if (final) {
        uploadFile.close();
        activeUpload = nullptr;

        String deckId = request->getParam("deckId", true)->value();
        // Final layout must match DeckStorage: <decks>/<deckId>/cards.jsonl
        // plus <decks>/<deckId>/deck-metadata.json (listDecks skips decks
        // whose metadata is missing or has an empty id).
        String deckDir = DeckStorage::getDeckPath(deckId);
        String finalPath = deckDir + "/cards.jsonl";

        if (!SdMan.ensureDirectoryExists(deckDir.c_str())) {
            SdMan.remove(uploadTempPath.c_str());
            sendJson(request, 500, "{\"error\":\"Failed to create deck directory\"}");
            return;
        }

        if (SdMan.exists(finalPath.c_str())) {
            SdMan.remove(finalPath.c_str());
        }

        if (!SdMan.rename(uploadTempPath.c_str(), finalPath.c_str())) {
            SdMan.remove(uploadTempPath.c_str());
            sendJson(request, 500, "{\"error\":\"Failed to finalize upload\"}");
            return;
        }

        // A re-upload replaces the card set, so any tracked scheduling state
        // may reference cards that no longer exist; drop it so countDue() and
        // the review stream agree on one card set.
        String progressPath = DeckStorage::getProgressPath(deckId);
        if (SdMan.exists(progressPath.c_str())) {
            SdMan.remove(progressPath.c_str());
        }

        if (uploadTotalBytes > 0 && uploadLastByte != '\n') {
            uploadLineCount++;  // last line without trailing newline
        }

        DeckMetadata meta;
        meta.id = deckId;
        meta.name = deckId;
        if (request->hasParam("name", true)) {
            String name = request->getParam("name", true)->value();
            name.trim();
            if (!name.isEmpty() && name.length() <= 128) {
                meta.name = name;
            }
        }
        meta.cardCount = (int)uploadLineCount;
        if (request->hasParam("cardCount", true)) {
            int count = request->getParam("cardCount", true)->value().toInt();
            if (count > 0) {
                meta.cardCount = count;
            }
        }
        meta.filePath = finalPath;

        // Write metadata atomically (like DeckStorage::saveProgress); on
        // failure keep the freshly installed cards.jsonl - the data is valid
        // and deleting it would destroy a previously working deck.
        String metaPath = deckDir + "/deck-metadata.json";
        String metaTmpPath = metaPath + ".tmp";
        bool metaOk = SdMan.writeFile(metaTmpPath.c_str(), meta.toJson());
        if (metaOk) {
            if (SdMan.exists(metaPath.c_str())) {
                SdMan.remove(metaPath.c_str());
            }
            metaOk = SdMan.rename(metaTmpPath.c_str(), metaPath.c_str());
        }
        if (!metaOk) {
            SdMan.remove(metaTmpPath.c_str());
            sendJson(request, 500, "{\"error\":\"Failed to write deck metadata\"}");
            return;
        }

        uploadCount = uploadCount + 1;
        Serial.println("WebServer: Upload complete for deck: " + deckId + " (" + String(uploadTotalBytes) +
                       " bytes, " + String(meta.cardCount) + " cards)");

        String response = "{\"success\":true,\"bytes\":" + String(uploadTotalBytes) + ",\"cards\":" +
                          String(meta.cardCount) + ",\"deckId\":\"" + deckId + "\"}";
        sendJson(request, 200, response);
    }
}

String WebServer::getMimeType(const String& path) {
    if (path.endsWith(".html")) return "text/html";
    if (path.endsWith(".htm")) return "text/html";
    if (path.endsWith(".css")) return "text/css";
    if (path.endsWith(".js")) return "application/javascript";
    if (path.endsWith(".json")) return "application/json";
    if (path.endsWith(".wasm")) return "application/wasm";
    if (path.endsWith(".png")) return "image/png";
    if (path.endsWith(".jpg")) return "image/jpeg";
    if (path.endsWith(".jpeg")) return "image/jpeg";
    if (path.endsWith(".gif")) return "image/gif";
    if (path.endsWith(".svg")) return "image/svg+xml";
    if (path.endsWith(".ico")) return "image/x-icon";
    if (path.endsWith(".woff")) return "font/woff";
    if (path.endsWith(".woff2")) return "font/woff2";
    if (path.endsWith(".ttf")) return "font/ttf";
    if (path.endsWith(".txt")) return "text/plain";
    return "application/octet-stream";
}
