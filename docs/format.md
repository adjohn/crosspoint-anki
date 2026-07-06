# JSON Format Specification

## Overview

The Anki app uses a streaming-friendly format to handle large decks without exhausting ESP32-C3 RAM (400KB total). Traditional JSON arrays require loading the entire deck into memory, which would crash the device with decks larger than ~500 cards.

**Solution**: Use **JSONL (Line-Delimited JSON)** format that allows streaming one card at a time.

## Deck Format

Each deck consists of two files:

1. `deck-metadata.json` - Small metadata file (fits in RAM)
2. `cards.jsonl` - Line-delimited JSON for cards (streamed from SD)

### deck-metadata.json

```json
{
  "id": "spanish-101",
  "name": "Spanish 101",
  "description": "Basic Spanish vocabulary",
  "cardCount": 150,
  "created": "2026-02-08",
  "tags": ["spanish", "vocabulary", "beginner"]
}
```

**Fields:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| id | string | Yes | Unique identifier for the deck (folder-safe) |
| name | string | Yes | Display name shown in deck list |
| description | string | No | Brief description |
| cardCount | number | Yes | Total number of cards |
| created | string | No | ISO 8601 date (YYYY-MM-DD) |
| tags | array | No | Array of string tags |

### cards.jsonl

Line-delimited JSON format. Each line is a valid JSON object representing one card:

```jsonl
{"id": "1", "front": "Hello", "back": "Hola", "tags": ["greetings"]}
{"id": "2", "front": "Goodbye", "back": "Adiós", "tags": ["greetings"]}
{"id": "3", "front": "Thank you", "back": "Gracias", "tags": ["courtesy"]}
{"id": "4", "front": "Please", "back": "Por favor", "tags": ["courtesy"]}
```

**Card Fields:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| id | string | Yes | Unique card identifier within deck |
| front | string | Yes | Plain-text content for card front |
| back | string | Yes | Plain-text content for card back |
| tags | array | No | Array of string tags for categorization |

**Notes:**
- Each line must be valid JSON
- No trailing commas
- UTF-8 encoding
- Front/back content is plain text - the device renders the strings verbatim
  (see HTML Handling section)

## Progress Format

### progress.json

```json
{
  "deckId": "spanish-101",
  "lastReview": 20492,
  "cards": {
    "1": {
      "ease": 2.36,
      "interval": 6,
      "repetitions": 2,
      "due": 20498
    },
    "2": {
      "ease": 1.7,
      "interval": 1,
      "repetitions": 0,
      "due": 20493
    }
  }
}
```

**Fields:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| deckId | string | Yes | Links to deck metadata |
| lastReview | number | No | Last review date as days since 1970-01-01, day boundary at the device's Day cutoff offset (UTC at the default UTC+0); 0 = never reviewed / unknown |
| cards | object | Yes | Map of cardId to progress data |

**Card Progress Fields:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| ease | number | Yes | Ease factor (floor 1.3, typically 1.3-3.0) |
| interval | number | Yes | Days until next review |
| repetitions | number | Yes | Consecutive correct reviews count |
| due | number | Yes | Next review date as days since 1970-01-01, day boundary at the device's Day cutoff offset (UTC at the default UTC+0); 0 = always due (new card or rated with no usable clock). A card rated Again keeps `due` = today until it earns Hard/Good/Easy |

**Migration:** older builds stored `lastReview`/`due` as `YYYY-MM-DD` strings.
Those files still load - the string dates parse as `0` ("always due" /
"never"), so each tracked card is offered once more and receives a numeric
due date on its next rating; ease, interval, and repetitions are preserved.

## HTML Handling

The device does **not** render HTML - card strings are drawn on the e-ink
screen exactly as they appear in `cards.jsonl`. Any markup left in a JSONL
file shows up literally. Embedded `\n` characters start a new line on
screen; text is word-wrapped to the display width and paginated when it
does not fit on one screen.

Anki notes, however, are full of HTML, so the browser upload page converts
`.apkg` field content to plain text before building the JSONL
(`web/js/apkg-parser.js`, `stripHtml`):

- `[sound:...]` references are removed
- `<br>` and closing block tags (`</div>`, `</p>`, `</li>`, `</ul>`,
  `</ol>`, `</tr>`, `</table>`, `</h1>`-`</h6>`, `</blockquote>`) become
  newlines
- All remaining tags are stripped (their text content is preserved)
- Named entities (`&nbsp;` `&lt;` `&gt;` `&quot;` `&amp;`) and numeric
  entities (`&#123;` / `&#x1F431;`) are decoded
- Whitespace is normalized (runs of spaces collapse, at most one blank line)

Example conversion: `<div>What is <b>2 + 2</b>?</div>` uploads as
`What is 2 + 2?`.

If you generate JSONL yourself (curl upload), supply plain text.

## Size Limits

**Maximum upload size: 10MB**

This limit prevents:
- RAM exhaustion during upload
- Excessive flash wear from large writes
- Timeout issues with slow SD cards

**Recommendations:**
- Split large decks (>1000 cards) into smaller topical decks
- Typical deck size: 100-500 cards
- Average card size: ~200 bytes (text only)

## Examples

### Complete Example Deck

**deck-metadata.json:**
```json
{
  "id": "spanish-basics",
  "name": "Spanish Basics",
  "description": "Essential Spanish vocabulary for beginners",
  "cardCount": 3,
  "created": "2026-02-08",
  "tags": ["spanish", "beginner"]
}
```

**cards.jsonl:**
```jsonl
{"id": "1", "front": "Hello", "back": "Hola", "tags": ["greetings"]}
{"id": "2", "front": "Goodbye", "back": "Adiós", "tags": ["greetings"]}
{"id": "3", "front": "Thank you\nvery much", "back": "Gracias", "tags": ["courtesy"]}
```

**progress.json (after some reviews):**
```json
{
  "deckId": "spanish-basics",
  "lastReview": 20492,
  "cards": {
    "1": {
      "ease": 2.36,
      "interval": 6,
      "repetitions": 2,
      "due": 20498
    },
    "2": {
      "ease": 2.36,
      "interval": 1,
      "repetitions": 1,
      "due": 20493
    },
    "3": {
      "ease": 1.7,
      "interval": 1,
      "repetitions": 0,
      "due": 20493
    }
  }
}
```

## Rationale for JSONL

### Traditional JSON Approach (Problematic)

```json
{
  "id": "spanish-101",
  "name": "Spanish 101",
  "cards": [
    {"id": "1", "front": "Hello", "back": "Hola"},
    {"id": "2", "front": "Goodbye", "back": "Adiós"},
    ...
    {"id": "1000", "front": "...", "back": "..."}
  ]
}
```

**Problems:**
- Must parse entire file to access any card
- All cards loaded into RAM simultaneously
- 1000 cards × 200 bytes = 200KB just for card data
- Plus JSON parsing overhead = ~400KB+ RAM usage
- **Result**: Crash on ESP32-C3 with 400KB total RAM

### JSONL Approach (Streaming)

```jsonl
{"id": "1", "front": "Hello", "back": "Hola"}
{"id": "2", "front": "Goodbye", "back": "Adiós"}
...
{"id": "1000", "front": "...", "back": "..."}
```

**Advantages:**
- Read one line at a time from SD card
- Parse one card at a time
- Process and immediately discard
- Memory usage: ~1KB constant (one card buffer)
- Deck size limited only by SD card capacity, not RAM
- **Result**: Works reliably on ESP32-C3

### Implementation Pattern

```cpp
// Open file for streaming
File cardFile = SD.open("/decks/spanish/cards.jsonl");

// Read cards one at a time
while (cardFile.available()) {
  String line = cardFile.readStringUntil('\n');
  Card card = parseCard(line);
  
  // Process card (display, update, etc.)
  processCard(card);
  
  // Card goes out of scope, memory freed
}
```

This streaming approach is essential for handling large Anki decks on resource-constrained embedded devices.
