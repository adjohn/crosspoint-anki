# Flashink

**Spaced-repetition flashcards for e-ink readers.** Flashink is a standalone
flashcard app for the Xteink X3 and X4 running [CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader) -
import your Anki `.apkg` decks over WiFi and study them with SM-2 scheduling,
right on the e-reader. Your device stays a normal CrossPoint e-reader;
Flashink is an app you launch from its Apps menu and exit back out of.

*Flashink is an independent community project. It imports the `.apkg` deck
format but is not affiliated with or endorsed by Anki/Ankitects Pty Ltd.*

## Features

- **Upload decks over WiFi** - the device hosts its own hotspot and web server (no router or internet needed); the browser page converts Anki `.apkg` exports on the fly
- **SM-2 spaced repetition** with real due dates - each session shows only the cards due today
- **In-session relearning** - cards rated Again come back later in the same session until you get them right, like desktop Anki
- **Local day cutoff** - due dates roll over at your local midnight (adjustable in the main menu; set automatically from your browser when you upload a deck)
- **Long cards page** - card text word-wraps and splits into pages you flip with the side buttons (or tilt on X3)
- **Multi-deck support** - browse and select from multiple decks
- **Deck management on device** - long-press Confirm on a deck to reset its progress or delete it
- **Offline capable** - works without internet
- **E-ink optimized** - UI designed for e-ink displays
- **Runs on X3 and X4** - single binary, automatic device detection at boot
- **Tilt gestures on X3** - navigate, rate, and page through cards by tilting the device
- **Progress persistence** - review state saved to SD card after every rating
- **Return to CrossPoint** - clean exit back to main launcher (menu item or long-press Back)

## Supported Hardware

| Device | Display | Extras |
|--------|---------|--------|
| Xteink X4 | 800x480 e-ink | - |
| Xteink X3 | 792x528 e-ink | Tilt gestures (gyro), fuel-gauge battery reporting, DS3231 RTC (keeps due dates across power-off) |

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
   /.crosspoint/apps/flashink/
     ├── app.bin
     └── app.json
   ```

2. Create deck directories:
   ```
   /.crosspoint/apps/flashink/
     ├── decks/       # Deck files will be stored here
     └── progress/    # Review progress stored here
   ```

3. Copy web files:
   ```
   /.crosspoint/apps/flashink/web/
     ├── upload.html
     ├── js/apkg-parser.js
     └── lib/
         ├── sql-wasm.js
         ├── sql-wasm.wasm
         └── jszip.min.js
   ```

4. Insert SD card into the device and power on

5. Launch Flashink from CrossPoint Apps menu

## Usage

### Main Menu

| Item | Action |
|------|--------|
| **Study** | Opens the deck list |
| **Upload Decks** | Starts the WiFi hotspot + upload web server |
| **Tilt: On/Off** | Toggles tilt gestures (X3 only - hidden on X4; persists across reboots) |
| **Day cutoff: UTC+H[:MM]** | The UTC offset at which review days roll over. **Left/Right** adjust in 30-minute steps (Confirm does nothing); clamped to UTC-12:00 ... UTC+14:00, persisted to NVS. Also set automatically from the browser on every deck upload |
| **Exit to CrossPoint** | Reboots back into the CrossPoint launcher |

### Uploading Decks

1. Select **Upload Decks** from the main menu. The device starts its own
   WiFi hotspot (no router involved) and shows an instruction screen with:
   - The network name: **Flashink-X3** on an X3, **Flashink-X4** on an X4
   - The password: **flashink123**
   - The address to open: **http://192.168.4.1** (plus `or http://flashink.local`
     when mDNS started successfully)
   - A **QR code** on the right side - scan it with a phone to join the
     hotspot without typing the password
   - A **"Decks uploaded: N"** counter that updates as decks arrive
2. Join the WiFi network from your computer or phone (scan the QR code or
   enter the credentials manually).
3. Open `http://192.168.4.1` (or `http://flashink.local`) in a browser. The
   upload page is served from the SD card (`/` redirects to `/upload.html`).
4. Upload your deck. Click the dashed **SELECT .APKG FILE** area and pick an
   Anki `.apkg` export (any other extension is rejected with "INVALID FILE
   TYPE. PLEASE USE .APKG" and the button stays disabled). Click **UPLOAD
   DECK**:
   - A progress bar appears - parsing drives it to 80%, the upload to the
     device takes it to 100%
   - The parser converts each card to plain text for the e-ink screen: HTML
     tags stripped (`<br>` becomes a line break), entities decoded,
     `[sound:...]` references removed; reversed cards keep their swapped
     front/back
   - On success: `SUCCESS: DECK "<name>" UPLOADED (N CARDS)`; the deck's
     display name is the `.apkg` file name (without extension) and the deck
     ID is a slug of it (lowercased, non-alphanumerics become dashes)
   - Each successful upload also silently sets the device's **Day cutoff**
     to your browser's current UTC offset (only when it differs), so due
     dates roll over at your local midnight without any on-device setup
   - On a server error (e.g. file over 10MB) the server's message is shown;
     if the request cannot reach the device at all the page shows
     "CONNECTION FAILED. CHECK YOU ARE ON THE DEVICE WIFI AND TRY AGAIN." -
     rejoin the hotspot and click UPLOAD DECK again
5. Alternatively, upload a deck already in the device's JSONL format via the
   HTTP API:
   ```bash
   curl -F "deckId=spanish-101" -F "name=Spanish 101" \
        -F "file=@cards.jsonl" http://192.168.4.1/upload-deck
   ```
   - `deckId` (required): 1-64 characters, letters/digits/`-`/`_` only
   - `name` (optional): display name shown in the deck list (defaults to `deckId`)
   - `cardCount` (optional): overrides the card count (defaults to the number
     of JSONL lines received)
   - `tzMinutes` (optional): sets the device's Day cutoff to this many
     minutes east of UTC (integer, clamped to -720...840); the browser page
     sends it automatically
   - Text fields must come **before** the `file` field - the firmware parses
     the multipart body sequentially and ignores fields after the file
   - Uploads are limited to 10MB; the server writes `cards.jsonl` and
     `deck-metadata.json` under `/.crosspoint/apps/flashink/decks/<deckId>/`
6. The "Decks uploaded" counter on the device increments after each
   successful upload. Press **Back** when done - this shuts down the web
   server, mDNS, and the hotspot, and returns to the main menu.

The 5-minute auto-sleep timer is suspended while the hotspot and web server
are running, so the device does not sleep mid-upload. (If the hotspot failed
to start - the "Could not start the WiFi hotspot" screen - auto-sleep works
as usual.) Sleeping from the upload screen, e.g. via a long press of the
power button, shuts down the hotspot, mDNS, and web server before the device
powers down.

### Studying

1. Select **Study** from the main menu, then pick a deck from the deck list
   (each row shows the deck name and how many cards are due today, e.g.
   "12 due" - new cards are always due).
2. **Review cards** - only cards due today are shown:
   - View the card front, press **Confirm** to reveal the answer
   - Press **Confirm** again to show the rating bar, then rate yourself:
     - **Left** = **Again** - forgot completely (repetitions reset; the card
       repeats later in the same session)
     - **Down** = **Hard** - remembered with difficulty (also a lapse, due
       tomorrow)
     - **Up** = **Good** - remembered with some effort
     - **Right** = **Easy** - remembered perfectly
   - Progress is saved to the SD card after every rating
3. **Long cards page** - card text word-wraps to the screen width; when it
   does not fit on one screen it splits into pages, with a page indicator
   ("2/5") in the top-right corner and a hint like "Confirm: reveal |
   Up/Down: page". **Up/Down** flip pages (no-op at the first/last page);
   on an X3, tilt forward/back also page - see the tilt table below.
   **Confirm** keeps its usual meaning on any page; revealing the answer
   always starts at page 1 of the back. On the answer screen the front is
   summarized in the top half (clipped if long - you already read it) and
   the paging applies to the answer text below the divider.
4. **Relearning** - after the last regularly due card, any cards you rated
   **Again** come back for another round (the header switches from
   "Review: <deck>" to "Relearning"). Each card repeats until you rate it
   Hard, Good, or Easy. If you exit mid-session, Again-rated cards stay due
   today and reappear next session.
5. **Session ends** when every due card has been rated and the relearning
   queue is empty; a summary screen shows cards **Reviewed** (unique cards,
   however often one repeated) and **Remaining** (cards still due today that
   were not rated this session). Opening a deck with nothing due shows
   "No cards due today".
6. Press **Confirm** or **Back** on the summary to return to the deck list.
   **Back** during a review also returns to the deck list (progress is kept).

#### Managing decks

Long-press **Confirm** (hold ~0.8s) on a deck in the deck list to open a
deck options box:

- **Reset progress** - deletes the deck's progress file immediately (no
  second confirmation); every card becomes due again as if freshly uploaded
- **Delete deck** - asks for a second **Confirm** on a confirmation box
  naming the deck; any other button cancels. Deletes the deck's files and
  its progress from the SD card
- **Cancel** (or **Back**) - closes the box with no action

In the options box **Up/Down** move the highlight, **Confirm** selects,
**Back** cancels; tilt gestures are ignored while it is open. A short press
of Confirm still opens the deck for review - it triggers on release, so
opening feels the same as before unless you keep holding. If a reset or
delete fails (SD error), a "Reset progress failed" / "Delete failed" message
appears at the bottom of the list until the next interaction.

#### Scheduling (SM-2)

Each rating updates the card's ease factor, repetition count, interval, and
due date:

- **Again** or **Hard** is a lapse: repetitions reset to 0 and the interval
  drops to **1 day**
- The first successful review (**Good**/**Easy**) schedules the card in
  **1 day**, the second in **6 days**; after that the previous interval is
  multiplied by the ease factor - a card rated Good every time runs
  1, 6, 12, 23, 41, ... days
- **Easy** raises the ease factor; **Good** lowers it slightly, **Hard** and
  **Again** lower it more (floor 1.3)

The due date is `today + interval`, except that **Again** keeps the card due
**today**: it re-enters the current session's relearning queue (and is still
due if you exit before re-rating it). Rating it Good/Easy during relearning
then schedules it normally (1 day for the first success after a lapse).
**Hard** schedules the card for tomorrow and does not repeat in-session.
Sessions skip cards that are not yet due. Note that every rating adjusts the
ease factor, including repeat Again ratings within one session - repeated
in-session lapses lower ease more than a single desktop-Anki lapse would.

**Day boundary** - a review "day" rolls over at the **Day cutoff** offset
(main menu), applied to the UTC clock: with the default UTC+0 days change at
UTC midnight; set it to your timezone and they change at your local
midnight. The offset is adjusted with Left/Right on the menu item (30-minute
steps, so half-hour zones like UTC+5:30 work) and is also set automatically
from the browser's timezone on every deck upload. Changing the offset
takes effect immediately and can shift which cards count as due today.

**Clock reality** - due dates need a real calendar date:

- **X3**: the app reads the date and time from the battery-backed DS3231 RTC
  (treated as UTC, like the system clock, with the Day cutoff applied on
  top) whenever the system clock is unset, so scheduling keeps working
  across power-off.
- **X4**: there is no RTC. The app relies on the ESP32 system clock, which
  CrossPoint typically sets via NTP (when it has WiFi) before launching the
  app. That clock survives a restart but **not** power-off/deep sleep or a
  cold boot.
- **No trustworthy clock**: the app degrades gracefully - every card is
  treated as due (each session reviews the whole deck) and ratings store a
  "due immediately" marker instead of a date. Scheduling resumes from the
  next rating made with a valid clock.

### Controls

| Button | Action |
|--------|--------|
| **Up/Down** (side buttons) | Move selection in menus and the deck list; previous/next page on multi-page cards |
| **Confirm** | Select menu item / open deck / reveal answer / show rating bar |
| **Long-press Confirm** (≥0.8s, deck list) | Open the deck options box (reset progress / delete deck) |
| **Back** | Go back one screen (deck list → main menu, review → deck list); cancels the deck options box |
| **Left/Right** | Adjust the Day cutoff (main menu, on that item only) |
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
| Review - question shown | Next page; on the last page, reveal answer | Previous page (page 1: ignored) |
| Review - answer shown | Next page; on the last page, rate **Good** | Previous page; on page 1, rate **Again** |
| Review - rating bar shown | Rate **Good** | Rate **Again** |
| Session complete / upload / deck options box | (ignored) | (ignored) |

On single-page cards "next page on the last page" collapses to the classic
behavior: forward reveals/rates Good, back rates Again.

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
  "lastReview": 20638,
  "cards": {
    "1": {"ease": 2.36, "interval": 6, "repetitions": 2, "due": 20644},
    "2": {"ease": 1.7, "interval": 1, "repetitions": 0, "due": 20639}
  }
}
```

`due` and `lastReview` are days since 1970-01-01, counted with the day
boundary at the configured Day cutoff offset (plain UTC epoch days at the
default UTC+0). A `due` of `0` means "always due" - used for cards rated
while no usable clock was available. Progress files written by older builds (which stored these fields
as strings) still load: the dates migrate to `0`, so each previously tracked
card is due once more and gets a real due date on its next rating (its ease,
interval, and repetitions are preserved).

## Technical Specifications

### Hardware Requirements

| | Xteink X4 | Xteink X3 |
|---|---|---|
| **SoC** | ESP32-C3 | ESP32-C3 |
| **Display** | 800x480 e-ink | 792x528 e-ink |
| **Battery reporting** | Analog (ADC) | BQ27220 fuel gauge (I2C) |
| **Tilt sensor** | - | QMI8658 gyro (I2C) |
| **RTC** | - (system clock only) | DS3231 (I2C, used for due dates) |

- **Device detection**: automatic at boot (I2C fingerprint probe, result cached in NVS namespace `cphw`)
- **RAM**: 400KB (app static usage ~95KB, ~29% of the 320KB DRAM pool)
- **Storage**: SD card for decks

### Performance
- **Build size**: ~1.35MB firmware binary (~20% of the app flash partition)
- **Deck size limit**: 10MB per upload
- **Card capacity**: Limited only by SD card size
- **Streaming**: Cards loaded one at a time (no RAM exhaustion); deck uploads stream straight to SD

### Supported Card Content
- Plain text (card front/back strings)
- Card text is word-wrapped to the screen width (UTF-8 aware; embedded line
  breaks respected; words wider than the screen split at glyph boundaries)
  and paginated when it does not fit on one screen - see
  [Studying](#studying)
- The browser upload page converts `.apkg` cards to plain text: HTML is
  stripped (`<br>` and block tags become line breaks), entities are decoded,
  and `[sound:...]` references are removed

*Note: images, audio, and cloze rendering are not supported; non-Latin
glyphs upload intact but may not be covered by the device fonts*

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

### Automated Tests

- `bash test/native/run.sh` - host-compiled unit tests exercising the real
  SM-2 scheduler, the timezone/day-cutoff math and DS3231 time+date
  register decoding, the relearning queue, the text
  wrapper (against a mock renderer), and the card/progress JSON
  (de)serialization + migration (currently 358 checks across 5 targets;
  needs `g++` and the ArduinoJson dependency fetched by a prior `pio run`)
- `bash test/web/run.sh` - end-to-end browser upload test: builds a real
  `.apkg` fixture, drives `web/upload.html` in headless Chromium against a
  mock server implementing the firmware's `/upload-deck` contract, and
  asserts the UI states, the exact JSONL bytes received, and the `tzMinutes`
  timezone field (currently 44 checks; needs `python3`, `node`, and
  Playwright with Chromium)

### Project Structure
```
firmware/
├── src/
│   ├── activities/     # UI screens (MainMenu, DeckList, Review, etc.)
│   ├── data/           # Card, Deck, Progress structures
│   ├── network/        # Web server for uploads
│   ├── scheduling/     # SM-2 algorithm + relearning queue
│   ├── storage/        # SD card I/O
│   └── utils/          # Boot, time/RTC + day cutoff, text wrapping
├── lib/                # GfxRenderer, fonts, etc.
└── open-x4-sdk/        # SDK submodule
```

## Troubleshooting

### "No decks found"
- Ensure decks are uploaded via the Upload Decks screen
- Check decks exist under `/.crosspoint/apps/flashink/decks/<deckId>/` with both
  `cards.jsonl` and `deck-metadata.json` (decks without valid metadata are skipped)

### "No cards due today"
- Not a bug: every card in that deck is scheduled for a future date. Come
  back when cards come due, or check `progress/<deckId>.json` for the `due`
  values (days since 1970-01-01)

### Cards come due at the wrong time of day
- Check the **Day cutoff** item in the main menu: due dates roll over at
  that UTC offset. It is set automatically from your browser's timezone on
  every deck upload, or adjust it with Left/Right on the menu item

### Cards you already reviewed keep coming back (X4)
- The X4 has no RTC, so due dates only work while the system clock is set
  (CrossPoint sets it via NTP when it has WiFi; the clock does not survive
  power-off). Without a clock the app intentionally reviews everything

### Upload fails
- Browser page says "CONNECTION FAILED...": your computer/phone dropped off
  the device hotspot (Flashink-X3 / Flashink-X4) - rejoin and click UPLOAD DECK again
- "INVALID FILE TYPE. PLEASE USE .APKG": the browser page only accepts
  `.apkg` files; JSONL decks go through the `curl` API instead
- `400 Missing deckId` / `Invalid deckId`: pass a `deckId` form field of 1-64
  letters, digits, `-` or `_` (the browser page derives a valid one
  automatically)
- `413 File too large`: uploads are capped at 10MB
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

Flashink stands on the shoulders of several projects:

- **[crosspoint-anki](https://github.com/DChells/crosspoint-x4-anki)** by
  DChells - the original Anki-for-CrossPoint app that Flashink grew from
- **[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)**
  by Dave Allie and the CrossPoint community - the e-reader firmware Flashink
  runs alongside, and the origin of the ported HAL, renderer, and tilt-sensor
  code (X3 device detection and gesture handling follow its implementation)
- **[OpenX4 Community SDK](https://github.com/open-x4-epaper/community-sdk)** -
  display, input, battery, and SD-card drivers for the Xteink hardware,
  including the X3 panel support
- **[Anki](https://apps.ankiweb.net/)** by Ankitects - the spaced-repetition
  software whose `.apkg` deck format Flashink imports (no affiliation)
- **SM-2** - the SuperMemo 2 scheduling algorithm by Piotr Wozniak
- **[sql.js](https://github.com/sql-js/sql.js)** and
  **[JSZip](https://stuk.github.io/jszip/)** - browser-side `.apkg` parsing
- **[ESPAsyncWebServer](https://github.com/mathieucarbou/ESPAsyncWebServer)**,
  **[ArduinoJson](https://arduinojson.org/)**, and
  **[QRCode](https://github.com/ricmoo/QRCode)** - the upload stack
- Bookerly, Noto Sans, Ubuntu, and OpenDyslexic fonts (bundled via
  CrossPoint's EpdFont library)
