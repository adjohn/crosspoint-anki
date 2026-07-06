# Anki - Flashcard App for CrossPoint E-Readers (Xteink X3 & X4)

A standalone Anki flashcard review app for Xteink e-ink e-readers. Study your Anki decks with SM-2 spaced repetition directly on the device.

## Features

- **Upload decks over WiFi** - the device hosts its own hotspot and web server (no router or internet needed)
- **SM-2 spaced repetition** algorithm for optimal learning
- **Multi-deck support** - browse and select from multiple decks
- **Offline capable** - works without internet
- **E-ink optimized** - UI designed for e-ink displays
- **Runs on X3 and X4** - single binary, automatic device detection at boot
- **Tilt gestures on X3** - navigate and rate cards by tilting the device
- **Progress persistence** - review state saved to SD card after every rating
- **Return to CrossPoint** - clean exit back to main launcher (menu item or long-press Back)

## Supported Hardware

| Device | Display | Extras |
|--------|---------|--------|
| Xteink X4 | 800x480 e-ink | - |
| Xteink X3 | 792x528 e-ink | Tilt gestures (gyro), fuel-gauge battery reporting |

The same `app.bin` runs on both devices. At boot the app detects the hardware
automatically (I2C fingerprint probe, cached in NVS) and configures the display
and inputs accordingly - no separate builds or configuration needed.

## Installation

### Prerequisites

- Xteink X3 or X4 e-reader with CrossPoint firmware
- MicroSD card (8GB+ recommended)
- A computer or phone with WiFi for deck uploads (the app creates its own hotspot; no router needed)

### Install App

1. Copy `firmware/app.bin` and `firmware/app.json` to SD card:
   ```
   /.crosspoint/apps/anki/
     ├── app.bin
     └── app.json
   ```

2. Create deck directories:
   ```
   /.crosspoint/apps/anki/
     ├── decks/       # Deck files will be stored here
     └── progress/    # Review progress stored here
   ```

3. Copy web files:
   ```
   /.crosspoint/apps/anki/web/
     ├── upload.html
     ├── js/apkg-parser.js
     └── lib/
         ├── sql-wasm.js
         ├── sql-wasm.wasm
         └── jszip.min.js
   ```

4. Insert SD card into the device and power on

5. Launch Anki from CrossPoint Apps menu

## Usage

### Main Menu

| Item | Action |
|------|--------|
| **Study** | Opens the deck list |
| **Upload Decks** | Starts the WiFi hotspot + upload web server |
| **Tilt: On/Off** | Toggles tilt gestures (X3 only - hidden on X4; persists across reboots) |
| **Exit to CrossPoint** | Reboots back into the CrossPoint launcher |

### Uploading Decks

1. Select **Upload Decks** from the main menu. The device starts its own
   WiFi hotspot (no router involved) and shows an instruction screen with:
   - The network name: **Anki-X3** on an X3, **Anki-X4** on an X4
   - The password: **ankideck123**
   - The address to open: **http://192.168.4.1** (plus `or http://anki.local`
     when mDNS started successfully)
   - A **QR code** on the right side - scan it with a phone to join the
     hotspot without typing the password
   - A **"Decks uploaded: N"** counter that updates as decks arrive
2. Join the WiFi network from your computer or phone (scan the QR code or
   enter the credentials manually).
3. Open `http://192.168.4.1` (or `http://anki.local`) in a browser. The
   upload page is served from the SD card (`/` redirects to `/upload.html`).
4. Upload your deck. **Note (v1):** the browser page's .apkg conversion and
   upload button are not wired up yet - selecting a file works but clicking
   "UPLOAD DECK" does not transfer anything. Until that lands, upload a deck
   in the device's JSONL format directly via the HTTP API:
   ```bash
   curl -F "deckId=spanish-101" -F "name=Spanish 101" \
        -F "file=@cards.jsonl" http://192.168.4.1/upload-deck
   ```
   - `deckId` (required): 1-64 characters, letters/digits/`-`/`_` only
   - `name` (optional): display name shown in the deck list (defaults to `deckId`)
   - `cardCount` (optional): overrides the card count (defaults to the number
     of JSONL lines received)
   - Uploads are limited to 10MB; the server writes `cards.jsonl` and
     `deck-metadata.json` under `/.crosspoint/apps/anki/decks/<deckId>/`
5. The "Decks uploaded" counter on the device increments after each
   successful upload. Press **Back** when done - this shuts down the web
   server, mDNS, and the hotspot, and returns to the main menu.

The 5-minute auto-sleep timer is suspended while the upload screen is open,
so the hotspot is not killed mid-upload.

### Studying

1. Select **Study** from the main menu, then pick a deck from the deck list
   (each row shows the deck name and card count).
2. **Review cards**:
   - View the card front, press **Confirm** to reveal the answer
   - Press **Confirm** again to show the rating bar, then rate yourself:
     - **Left** = **Again** - forgot completely (repetitions reset)
     - **Down** = **Hard** - remembered with difficulty
     - **Up** = **Good** - remembered with some effort
     - **Right** = **Easy** - remembered perfectly
   - Progress is saved to the SD card after every rating
3. **Session ends** after the last card is rated; a summary screen shows
   cards **Reviewed** and **Remaining**. (v1 reviews every card in the deck
   in order - there is no due-date filtering yet.)
4. Press **Confirm** or **Back** on the summary to return to the deck list.
   **Back** during a review also returns to the deck list (progress is kept).

### Controls

| Button | Action |
|--------|--------|
| **Up/Down** (side buttons) | Move selection in menus and the deck list |
| **Confirm** | Select menu item / reveal answer / show rating bar |
| **Back** | Go back one screen (deck list → main menu, review → deck list) |
| **Left / Down / Up / Right** | Rate Again / Hard / Good / Easy (rating bar shown) |
| **Long-press Back** (≥1.2s) | Exit to CrossPoint - works from any screen |
| **Long-press Power** (≥1s) | Deep sleep ("Sleeping..." screen) |

The four front buttons (Back/Confirm/Left/Right) follow your CrossPoint
front-button remap settings; the side buttons are always Up/Down.

The device auto-sleeps after **5 minutes** without a button press or tilt
gesture. The upload screen suppresses auto-sleep while it is open.

#### Tilt Gestures (X3 only)

The X3's built-in gyro enables hands-free-ish control. Gestures can be turned
on/off via the **Tilt: On/Off** item in the main menu (the setting persists
across reboots; default is on). The menu item only appears on an X3.

| Screen | Tilt forward | Tilt back |
|--------|--------------|-----------|
| Main menu / deck list | Selection down | Selection up |
| Review - question shown | Reveal answer | (ignored) |
| Review - answer/rating shown | Rate **Good** | Rate **Again** |
| Session complete / upload screen | (ignored) | (ignored) |

Tilt gestures also count as activity for the auto-sleep timer. Buttons always
work regardless of the tilt setting.

## Deck Format

Decks are stored in JSONL format (one card per line) for memory efficiency:

```
deck-metadata.json:
{
  "id": "spanish-101",
  "name": "Spanish 101",
  "cardCount": 150
}

cards.jsonl (one card per line):
{"id": "1", "front": "Hello", "back": "Hola", "tags": ["greetings"]}
{"id": "2", "front": "Goodbye", "back": "Adiós", "tags": ["greetings"]}
```

Progress is stored separately at `progress/<deckId>.json`:
```
{
  "deckId": "spanish-101",
  "lastReview": "",
  "cards": {
    "1": {"ease": 2.5, "interval": 6, "repetitions": 3, "due": ""},
    "2": {"ease": 2.3, "interval": 1, "repetitions": 0, "due": ""}
  }
}
```

The `due` and `lastReview` fields are reserved: v1 updates ease, interval,
and repetitions after each rating but does not yet write due dates or filter
cards by them - every session walks the whole deck.

## Technical Specifications

### Hardware Requirements

| | Xteink X4 | Xteink X3 |
|---|---|---|
| **SoC** | ESP32-C3 | ESP32-C3 |
| **Display** | 800x480 e-ink | 792x528 e-ink |
| **Battery reporting** | Analog (ADC) | BQ27220 fuel gauge (I2C) |
| **Tilt sensor** | - | QMI8658 gyro (I2C) |

- **Device detection**: automatic at boot (I2C fingerprint probe, result cached in NVS namespace `cphw`)
- **RAM**: 400KB (app static usage ~95KB, ~29% of the 320KB DRAM pool)
- **Storage**: SD card for decks

### Performance
- **Build size**: ~1.35MB firmware binary (~20% of the app flash partition)
- **Deck size limit**: 10MB per upload
- **Card capacity**: Limited only by SD card size
- **Streaming**: Cards loaded one at a time (no RAM exhaustion); deck uploads stream straight to SD

### Supported Card Content
- Plain text (card front/back strings are rendered as-is)

*Note: HTML/markdown formatting, images, audio, and long-text wrapping are
not supported in v1 - very long card text may clip at the screen edge*

## Building from Source

### Requirements
- PlatformIO Core
- Python 3.8+
- ESP32-C3 toolchain

### Build
```bash
cd firmware
source ../.venv/bin/activate
pio run
```

The compiled binary will be at `.pio/build/default/firmware.bin`

### Project Structure
```
firmware/
├── src/
│   ├── activities/     # UI screens (MainMenu, DeckList, Review, etc.)
│   ├── data/           # Card, Deck, Progress structures
│   ├── network/        # Web server for uploads
│   ├── scheduling/     # SM-2 algorithm
│   ├── storage/        # SD card I/O
│   └── utils/          # Boot utilities
├── lib/                # GfxRenderer, fonts, etc.
└── open-x4-sdk/        # SDK submodule
```

## Troubleshooting

### "No decks found"
- Ensure decks are uploaded via the Upload Decks screen
- Check decks exist under `/.crosspoint/apps/anki/decks/<deckId>/` with both
  `cards.jsonl` and `deck-metadata.json` (decks without valid metadata are skipped)

### Upload fails
- `400 Missing deckId` / `Invalid deckId`: pass a `deckId` form field of 1-64
  letters, digits, `-` or `_`
- `413 File too large`: uploads are capped at 10MB
- Verify your computer is connected to the device hotspot (Anki-X3 / Anki-X4)
- The device screen shows "Could not start the WiFi hotspot" if the AP or web
  server failed to start - press Back and try again

### App crashes
- Check SD card is properly formatted (FAT32)
- Verify deck JSON is valid
- Try smaller deck (< 1000 cards)

## License

MIT License - See LICENSE file

## Contributing

This is a community project for Xteink e-readers. Contributions welcome!

## Acknowledgments

- Anki - The original spaced repetition software
- OpenX4 Community SDK - Hardware abstraction layer
- sql.js - SQLite in JavaScript for browser-side parsing
