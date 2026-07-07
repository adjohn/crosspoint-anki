#pragma once
#include <WString.h>
#include <vector>
#include <ArduinoJson.h>

struct Card {
    String id;
    String front;
    String back;
    std::vector<String> tags;
    
    // Parse from JSONL line
    static Card fromJson(const String& jsonLine) {
        Card card;
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, jsonLine);
        
        if (!error) {
            // `| ""` so absent keys parse to empty, not the text "null"
            card.id = doc["id"] | "";
            card.front = doc["front"] | "";
            card.back = doc["back"] | "";
            
            JsonArray tagsArray = doc["tags"];
            for (JsonVariant tag : tagsArray) {
                card.tags.push_back(tag.as<String>());
            }
        }
        return card;
    }
    
    // Serialize to JSONL line
    String toJson() const {
        JsonDocument doc;
        doc["id"] = id;
        doc["front"] = front;
        doc["back"] = back;
        
        JsonArray tagsArray = doc["tags"].to<JsonArray>();
        for (const String& tag : tags) {
            tagsArray.add(tag);
        }
        
        String output;
        serializeJson(doc, output);
        return output;
    }
};
