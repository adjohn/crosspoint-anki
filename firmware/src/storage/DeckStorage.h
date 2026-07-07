#pragma once
#include <Arduino.h>
#include "../data/DeckMetadata.h"
#include "../data/Card.h"
#include "../data/Progress.h"
#include <SDCardManager.h>
#include <vector>

class CardStream {
private:
    FsFile file;
    
public:
    bool open(const String& deckId);
    bool hasNext();
    Card readNext();
    void close();
};

class DeckStorage {
public:
    static std::vector<DeckMetadata> listDecks();
    static DeckMetadata loadDeckMetadata(const String& deckId);
    static bool openCardStream(const String& deckId, CardStream& stream);
    static DeckProgress loadProgress(const String& deckId);
    static bool saveProgress(const String& deckId, const DeckProgress& progress);
    // Removes the deck directory (cards.jsonl, deck-metadata.json, anything
    // else inside) and the progress file. Returns false if anything remains.
    static bool deleteDeck(const String& deckId);
    // Removes the progress file only; missing file counts as success.
    static bool resetProgress(const String& deckId);
    static String getDecksDir();
    static String getDeckPath(const String& deckId);
    static String getProgressPath(const String& deckId);
};
