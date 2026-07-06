# Anki - Flashcard App for CrossPoint E-Readers (Xteink X3 & X4)

A standalone Anki flashcard review app for Xteink e-ink e-readers. Study your Anki decks with SM-2 spaced repetition directly on the device.

## Features

- **Upload Anki decks** via browser (converts .apkg to device format)
- **SM-2 spaced repetition** algorithm for optimal learning
- **Multi-deck support** - browse and select from multiple decks
- **Offline capable** - works without internet after initial setup
- **E-ink optimized** - UI designed for e-ink displays
- **Runs on X3 and X4** - single binary, automatic device detection at boot
- **Tilt gestures on X3** - navigate and rate cards by tilting the device
- **Progress persistence** - review state saved to SD card
- **Return to CrossPoint** - clean exit back to main launcher

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
- WiFi network for initial setup

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

### Uploading Decks

1. **Connect to device WiFi**:
   - The device creates a WiFi AP when the Anki app starts
   - The AP name and hostname include "Anki" (see the device screen for the exact name and password)

2. **Open browser** and navigate to:
   - `http://192.168.4.1` (or the address shown on the device)

3. **Upload .apkg file**:
   - Select your Anki deck (.apkg file)
   - Click "Upload"
   - The file is automatically converted and saved to SD card

### Studying

1. **Select deck** from the deck list
2. **Review cards**:
   - View card front
   - Press **Confirm** to reveal answer
   - Rate yourself:
     - **Again** (0) - Forgot completely
     - **Hard** (2) - Remembered with difficulty
     - **Good** (3) - Remembered with some effort
     - **Easy** (5) - Remembered perfectly
3. **Session ends** when all due cards are reviewed
4. **Return to deck list** or **exit to CrossPoint**

### Controls

| Button | Action |
|--------|--------|
| **Up/Down** | Navigate menus |
| **Confirm** | Select / Reveal answer |
| **Back** | Go back / Exit menu |
| **Long-press Back** | Exit to CrossPoint |

#### Tilt Gestures (X3 only)

The X3's built-in gyro enables hands-free-ish control. Gestures can be turned
on/off via the **Tilt: On/Off** item in the main menu (the setting persists
across reboots; default is on). The menu item only appears on an X3.

| Screen | Tilt forward | Tilt back |
|--------|--------------|-----------|
| Main menu / deck list | Selection down | Selection up |
| Review - question shown | Reveal answer | (ignored) |
| Review - answer/rating shown | Rate **Good** | Rate **Again** |
| Session complete | (ignored) | (ignored) |

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

Progress is stored separately:
```
progress.json:
{
  "deckId": "spanish-101",
  "cards": {
    "1": {"ease": 2.5, "interval": 6, "repetitions": 3, "due": "2026-02-14"},
    "2": {"ease": 2.3, "interval": 1, "repetitions": 0, "due": "2026-02-09"}
  }
}
```

## Technical Specifications

### Hardware Requirements

| | Xteink X4 | Xteink X3 |
|---|---|---|
| **SoC** | ESP32-C3 | ESP32-C3 |
| **Display** | 800x480 e-ink | 792x528 e-ink |
| **Battery reporting** | Analog (ADC) | BQ27220 fuel gauge (I2C) |
| **Tilt sensor** | - | QMI8658 gyro (I2C) |

- **Device detection**: automatic at boot (I2C fingerprint probe, result cached in NVS namespace `cphw`)
- **RAM**: 400KB (app uses ~20KB)
- **Storage**: SD card for decks

### Performance
- **Build size**: 356KB flash, 19.5KB RAM
- **Deck size limit**: 10MB per upload
- **Card capacity**: Limited only by SD card size
- **Streaming**: Cards loaded one at a time (no RAM exhaustion)

### Supported Card Content
- Plain text
- **Bold** and *italic* formatting
- Line breaks
- Basic lists

*Note: Images, audio, and complex HTML are not supported in v1*

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
- Ensure decks are uploaded via web interface
- Check SD card is mounted at `/.crosspoint/apps/anki/decks/`

### Upload fails
- Check file is valid .apkg format
- Ensure file is under 10MB
- Verify WiFi connection to device

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
