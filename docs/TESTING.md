# Flashink Testing Guide

This document provides comprehensive testing instructions for Flashink, a spaced-repetition flashcard app for CrossPoint devices (Xteink X3 and X4). The same `app.bin` runs on both devices; unless a step says otherwise, run the tests on whichever device you have. X3-specific tests are in [X3-Specific Tests](#x3-specific-tests).

---

## Prerequisites

- Xteink X3 or X4 device with CrossPoint firmware 0.16.0+ (PR #679 app extension support)
- A computer or phone with WiFi (deck uploads use a hotspot the device creates itself; a shared WiFi network is only needed if you install the app via CrossPoint File Transfer in STA mode)
- A test deck: a real Anki `.apkg` export (for the browser upload) and/or the JSONL sample below (for the curl API) - see [Test Decks](#test-decks)
- Battery > 20% (required for app installation)

---

## Automated Test Suites

Run these on a computer (no device needed); both exit nonzero on failure.

| Suite | Command | Coverage | Current status |
|-------|---------|----------|----------------|
| Native unit tests | `bash test/native/run.sh` | Real `SM2.cpp` interval/ease math; `TimeUtils` BCD + DS3231 time/date register decoding (24h and 12h modes), timezone-offset clamping and local-day-boundary conversion; real `RelearnQueue.h` (enqueue/dedup/dequeue, 256-ID cap) plus mirrors of the review relearn-pass loop; real `TextWrap.cpp` word wrap against a mock renderer (UTF-8 boundary safety, long-word splitting, newline handling); `Card`/`Progress` JSON parsing and old-format migration (compiled host-side against an Arduino shim) | 5 targets, 358 checks passing (test_sm2 96, test_timeutils 76, test_relearn 64, test_textwrap 56, test_card_json 66) |
| Web upload E2E | `bash test/web/run.sh` | Builds a real `.apkg` fixture (plain, HTML+entities, unicode, reversed cards), drives `web/upload.html` in headless Chromium against a mock server implementing the firmware `/upload-deck` contract, asserts the UI states, the exact JSONL bytes received, and the `tzMinutes` timezone field (present before the file part, integer, matching the browser's UTC offset) | 44 checks passing |

Native suite needs `g++` plus the ArduinoJson dependency (`pio run` once in
`firmware/` fetches it, otherwise the JSON target is skipped and counted as a
failure). Web suite needs `python3`, `node`, and Playwright with Chromium.

---

## Test Decks

### Create a minimal test deck (JSONL)

The device stores decks in JSONL format (one card per line). Create a file
`cards.jsonl` on your computer:

```
{"id": "1", "front": "Hello", "back": "Hola", "tags": ["greetings"]}
{"id": "2", "front": "Goodbye", "back": "Adios", "tags": ["greetings"]}
{"id": "3", "front": "Thank you", "back": "Gracias", "tags": ["greetings"]}
{"id": "4", "front": "Please", "back": "Por favor", "tags": ["greetings"]}
{"id": "5", "front": "Yes / No", "back": "Si / No", "tags": ["basics"]}
```

You upload it with `curl` in Test 2 below.

### Known Limitations
- **Max upload size**: 10MB
- **Text only**: no images or audio; the browser upload strips HTML to plain
  text and drops `[sound:...]` references
- **Device fonts**: unicode uploads intact (verified by the web E2E suite)
  but non-Latin glyphs may not render on the e-ink screen
- **Cloze notes are approximated**: each Anki card row uploads with its raw
  field text, not cloze-rendered text
- **Ease drops on every Again**: a card rated Again repeats later in the
  same session (see Test 6), and each repeat lowers its ease factor again -
  desktop Anki penalizes ease only once per lapse
- **X4 due dates need the system clock**: the X4 has no RTC, so due-date
  filtering only works while the ESP32 clock is set (CrossPoint NTP); with
  no usable clock every session reviews all cards. The X3 uses its DS3231
  RTC and keeps dates across power-off. See Test 5.

---

## Installation Test

### 1. Download the Release

```
this repository's Releases page
```

Download: `flashink-v0.2.0.zip`

### 2. Upload via CrossPoint File Transfer

| Step | Action | Expected Result |
|------|--------|-----------------|
| 2.1 | On the device: Home → File Transfer | File Transfer screen appears |
| 2.2 | Connect to WiFi (STA) or create hotspot (AP) | IP address displayed on screen |
| 2.3 | On computer: open the URL shown | CrossPoint web interface loads |
| 2.4 | Click **Apps** tab | Apps management page opens |
| 2.5 | Upload `flashink-v0.2.0.zip` | Upload progress shown, success message |

**Verify**: App appears in the list with name "Flashink"

### 3. Install the App

| Step | Action | Expected Result |
|------|--------|-----------------|
| 3.1 | On the device: Home → Apps | Apps list shows "Flashink" |
| 3.2 | Select "Flashink" | App details screen |
| 3.3 | Press Install | Progress indicator, device reboots |
| 3.4 | Wait for reboot | Flashink main menu appears |

**Checkpoint**: If installation fails, note:
- Battery level
- Error message (if any)
- Device behavior

---

## App Functionality Tests

### Test 1: Main Menu Navigation

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1.1 | App boots | "Flashink" header, menu items visible |
| 1.2 | Observe menu items | X4: "Study", "Upload Decks", "Day cutoff: UTC+0", "Exit to CrossPoint". X3 adds "Tilt: On" (or "Tilt: Off") between "Upload Decks" and the Day cutoff item. (The Day cutoff label shows whatever offset is stored - "UTC+0" only on a fresh install; see Test 8) |
| 1.3 | Press side Up/Down buttons | Highlight bar moves between options |

**Report**:
- [ ] PASS - Menu displays correctly
- [ ] FAIL - Describe issue: _______________

### Test 2: Deck Upload (WiFi)

| Step | Action | Expected Result |
|------|--------|-----------------|
| 2.1 | Select "Upload Decks" | Instruction screen: network **Flashink-X3** (X3) or **Flashink-X4** (X4), password **flashink123**, `http://192.168.4.1` (plus "or http://flashink.local" if mDNS started), "Decks uploaded: 0", "Press Back when done", and a QR code on the right captioned "Scan to join WiFi" |
| 2.2 | On a phone: scan the QR code | Phone joins the Flashink-X3/Flashink-X4 hotspot without typing the password |
| 2.3 | On computer: join the hotspot manually and open `http://192.168.4.1` | Upload page ("Flashink — Upload Deck") loads; `/` redirects to `/upload.html` |
| 2.4 | Click the dashed area and select a real Anki `.apkg` export | Label changes to "FILE: <name>"; UPLOAD DECK button becomes enabled |
| 2.5 | Click UPLOAD DECK | Progress bar appears (parsing drives 0-80%, upload the rest); on completion an underlined message: `SUCCESS: DECK "<name>" UPLOADED (N CARDS)` where the name is the `.apkg` file name without extension; button re-enabled |
| 2.6 | Watch the device screen (updates within ~1 second) | "Decks uploaded: 1" |
| 2.7 | Upload the JSONL test deck via the HTTP API: `curl -F "deckId=test-deck" -F "name=Test Deck" -F "file=@cards.jsonl" http://192.168.4.1/upload-deck` | JSON response `{"success":true, "bytes":..., "cards":5, "deckId":"test-deck"}`; counter increments again |
| 2.8 | (Optional) Leave the upload screen idle for over 5 minutes | Device does NOT auto-sleep while the hotspot/web server is running; on other screens 5 idle minutes shows "Sleeping..." and sleeps |
| 2.9 | (Optional) Long-press Power on the upload screen | Hotspot, mDNS, and web server shut down first (the WiFi network disappears), then "Sleeping..." is shown and the device sleeps |
| 2.10 | Press Back, then select "Study" | Hotspot shuts down; "Test Deck" visible in deck list with "5 due" (new cards are all due) |

**Note**: the browser page accepts only `.apkg` files; JSONL decks go through
the curl API. Cards uploaded from an `.apkg` are converted to plain text
(HTML stripped, entities decoded, `[sound:...]` removed) and reversed cards
arrive with front/back swapped.

**Report**:
- [ ] PASS - Deck uploads and appears in list
- [ ] FAIL - Describe issue: _______________
- Upload time for test deck: _______ seconds
- Deck name displayed correctly: [ ] Yes [ ] No

### Test 3: Deck List

| Step | Action | Expected Result |
|------|--------|-----------------|
| 3.1 | Select "Study" from main menu | "Select Deck" list appears |
| 3.2 | Observe deck info | Deck name on the left, due count ("N due") on the right - computed from the progress file, so a fresh deck shows all its cards due |
| 3.3 | Navigate between decks (if multiple) | Selection moves correctly; list scrolls with a scrollbar when it overflows |
| 3.4 | Press Confirm on a deck | Review session starts (card front shown) |
| 3.5 | Press Back from the deck list | Returns to main menu |

**Report**:
- [ ] PASS - Deck list works correctly
- [ ] FAIL - Describe issue: _______________

### Test 4: Review Session (Core Functionality)

| Step | Action | Expected Result |
|------|--------|-----------------|
| 4.1 | Start review | Header "Review: <deckId>"; card front (question) displayed, hint "Press Confirm to reveal" (multi-page cards show a different hint - see Test 7) |
| 4.2 | Press Confirm | Card back (answer) displayed below the front, hint "Press Confirm to rate" |
| 4.3 | Press Confirm again | Rating bar at the bottom: "Again", "Hard", "Good", "Easy" |
| 4.4 | Rate card as "Good" (press side Up button) | Next card's front appears |
| 4.5 | Try the other ratings on later cards (Left=Again, Down=Hard, Right=Easy) | Each rating advances to the next card; Again-rated cards come back after the last due card (see Test 6) |
| 4.6 | Rate the last card Good/Easy (with no Again ratings pending) | Session complete screen appears automatically |
| 4.7 | Press Back mid-session (on a later run) | Returns to deck list; ratings so far are kept |

**Report**:
- [ ] PASS - Review flow works correctly
- [ ] FAIL - Describe issue: _______________

**Text rendering**:
- [ ] Text readable and properly sized
- [ ] Long text wraps correctly
- [ ] Special characters display correctly (if applicable)

### Test 5: SM-2 Scheduling and Due Dates

This tests spaced repetition logic. Intervals are not shown on screen; verify
them in the progress file on the SD card
(`/.crosspoint/apps/flashink/progress/<deckId>.json`). `due` and `lastReview` are
**days since 1970-01-01**, with the day boundary shifted by the Day cutoff
offset (plain UTC days at the default UTC+0 - see Test 8); `due: 0` means
"always due" (a rating made with no usable clock).

| Step | Action | Expected Result |
|------|--------|-----------------|
| 5.1 | Review a new card, rate "Good" | Progress file: `repetitions` = 1, `interval` = 1, `ease` ≈ 2.36 (Good nudges ease down from the 2.5 start), `due` = today + 1 |
| 5.2 | Review the same card when due again, rate "Good" | `repetitions` = 2, `interval` = 6, `due` = today + 6 |
| 5.3 | Third "Good" | `repetitions` = 3, `interval` = round(6 x ease) - the all-Good ladder runs 1, 6, 12, 23, 41, ... days |
| 5.4 | Rate a card "Hard" | `repetitions` resets to 0, `interval` = 1, `due` = tomorrow, `ease` drops (never below 1.3). The card does NOT reappear in the same session |
| 5.4b | Rate a card "Again" | `repetitions` resets to 0, `interval` = 1, but `due` = **today**: the card reappears later in the same session (Test 6) and, if you exit before re-rating it, in the next session too. `ease` drops on every Again |
| 5.5 | Rate a card "Easy" | `ease` increases (ends above cards rated "Good") |
| 5.6 | Rate every card in a small deck Good/Easy/Hard, then reopen the same deck | "No cards due today" screen immediately; deck list shows "0 due" for it |
| 5.7 | (X3) Power the device off overnight, reopen the deck next day | Cards that were due "tomorrow" are offered again - the DS3231 RTC keeps the date across power-off |
| 5.8 | (X4) Cold-boot without CrossPoint ever having WiFi/NTP, open a deck | No usable clock: EVERY card is reviewed regardless of stored due dates, and new ratings write `due: 0`. Once the clock is set again (CrossPoint NTP), sessions filter by due date and the next rating writes a real date |

**Report**:
- [ ] PASS - Progress file values change as described
- [ ] PASS - Rated cards disappear from sessions until due (5.6)
- [ ] FAIL - Describe issue: _______________

### Test 6: In-Session Relearning (Again Cards)

Cards rated **Again** stay due today and repeat within the session until
they earn Hard, Good, or Easy - like desktop Anki's relearning step.

| Step | Action | Expected Result |
|------|--------|-----------------|
| 6.1 | Start a session on a deck with 3+ due cards; rate the FIRST card "Again" and the rest "Good" | The session does not end after the last due card - the Again-rated card comes back |
| 6.2 | Observe the header when it comes back | Header reads "Relearning" instead of "Review: <deckId>"; reveal/rating/paging work exactly as before |
| 6.3 | Rate the relearning card "Again" once more | It comes back again (`ease` in the progress file drops on every Again) |
| 6.4 | Rate it "Good" | "Session Complete!" appears. Reviewed counts each card ONCE however often it repeated (3 due cards → "Reviewed: 3, Remaining: 0") |
| 6.5 | Rate several cards Again in one session | After the main pass they return in deck-file order, in repeated passes, until each one is rated Hard/Good/Easy |
| 6.6 | During relearning, rate a card "Hard" | It does NOT return this session; progress file shows `due` = tomorrow |
| 6.7 | Rate a card Again, press Back mid-relearning, reopen the deck | The card is offered again immediately (it stayed due today; deck list still counts it due) |

**Note**: the relearning queue holds at most 256 distinct cards per session;
beyond that, extra Again-rated cards do not repeat in-session but stay due
today and are caught next session. The cap, dedup, and dequeue logic are
covered by the automated `test_relearn` target (not practical on-device).

**Report**:
- [ ] PASS - Again cards repeat until passed; counts correct
- [ ] FAIL - Describe issue: _______________

### Test 7: Long-Card Paging

Prepare a deck with a card whose front and back are each 500+ characters
(e.g. add a long line to `cards.jsonl` and upload via curl). Run on both
device models if available - the X3's taller panel (528 vs 480 px) fits
more lines per page.

| Step | Action | Expected Result |
|------|--------|-----------------|
| 7.1 | Open the long card | Text word-wrapped inside 10px side margins, nothing clipped at the right edge; page indicator "1/N" in the top-right of the header; hint reads "Confirm: reveal \| Up/Down: page" |
| 7.2 | Press Down, then Up | Next / previous page, indicator updates; pressing Up on page 1 or Down on the last page does nothing (no e-ink refresh) |
| 7.3 | Press Confirm from a middle page | Answer revealed regardless of page; the back starts at page 1 with its own "1/M" indicator and hint "Confirm: rate \| Up/Down: page" |
| 7.4 | On the back: page around, then press Confirm and rate | Rating bar overlays only the bottom strip (never covers card text); while it is shown Up/Down rate Good/Hard - paging is disabled |
| 7.5 | Observe the back screen's top half | The front is summarized above the divider, clipped (not paginated) if it is long - you already paged through it |
| 7.6 | Open a short (single-screen) card | No page indicator; original hints "Press Confirm to reveal" / "Press Confirm to rate" |
| 7.7 | Card with embedded line breaks (from `<br>` / block tags) | Breaks preserved; blank lines render as blank lines |

X3 tilt paging is covered in Test X3-3 (steps X3-3.5 to X3-3.7). The wrap
algorithm itself (UTF-8 boundaries, long-word splitting, newline handling)
is covered by the automated `test_textwrap` target.

**Report**:
- [ ] PASS - Long cards wrap and page correctly on the tested device(s)
- [ ] FAIL - Describe issue: _______________

### Test 8: Day Cutoff (Local-Time Day Boundary)

Review "days" roll over at a configurable UTC offset instead of fixed UTC
midnight. The offset lives in the main menu and is also auto-set from the
browser on every deck upload.

| Step | Action | Expected Result |
|------|--------|-----------------|
| 8.1 | Main menu: highlight the "Day cutoff" item, press Right twice | Label advances by 30 minutes per press (e.g. "UTC+0" → "UTC+0:30" → "UTC+1"); whole hours show no minutes |
| 8.2 | Hold down Left past UTC-12, then Right past UTC+14 | Clamps at "Day cutoff: UTC-12" and "Day cutoff: UTC+14" - no wrap-around |
| 8.3 | Press Confirm on the item | Nothing happens (Left/Right are the only adjustment inputs) |
| 8.4 | Set a non-zero offset, reboot the app | Offset retained (persisted to NVS) |
| 8.5 | Upload any deck from the browser page (Test 2 flow) | On success the Day cutoff silently becomes the browser's current UTC offset (check the main menu label; unchanged if it already matched) |
| 8.6 | Upload via curl with an explicit offset: `curl -F "deckId=tz-deck" -F "tzMinutes=330" -F "file=@cards.jsonl" http://192.168.4.1/upload-deck` (text fields must precede `file`) | Menu shows "Day cutoff: UTC+5:30". Out-of-range values clamp to the -720...840 range; non-integer values are ignored; a failed upload never changes the offset |
| 8.7 | Rate a card, then move the cutoff so the local day flips (e.g. at 23:00 UTC, UTC+0 → UTC+2) | Due counts and session filtering immediately use the new local day - a card due "tomorrow" can become due now. Restore your real offset afterwards |

The boundary math (clamping, half-hour zones, DS3231 time-of-day decoding,
RTC/system-clock consistency) is covered by the automated `test_timeutils`
target; the browser field format by the web E2E suite.

**Report**:
- [ ] PASS - Cutoff adjusts, persists, and auto-sets from uploads
- [ ] FAIL - Describe issue: _______________

### Test 9: Deck Management (Reset / Delete)

| Step | Action | Expected Result |
|------|--------|-----------------|
| 9.1 | Deck list: press and HOLD Confirm on a deck for ~1s | After 0.8s an options box pops over the list: deck name as title, items "Reset progress" / "Delete deck" / "Cancel". Releasing the button afterwards does nothing extra |
| 9.2 | Short-press Confirm instead (release quickly) | Deck opens for review as usual (opening triggers on release, so it feels immediate) |
| 9.3 | In the box: press Up/Down; try Left/Right; on an X3, tilt | Highlight moves and wraps between the three items; Left/Right do nothing; tilt gestures are ignored while the box is open |
| 9.4 | Select "Cancel" (or press Back) | Box closes, deck list unchanged |
| 9.5 | Long-press again, select "Reset progress" on a partially reviewed deck | Acts immediately (no second confirmation); list reloads with the deck showing its full card count due; `progress/<deckId>.json` is gone from the SD card |
| 9.6 | Long-press again, select "Delete deck" | Box switches to a confirmation: "Press Confirm again to delete", the deck name in quotes, "Any other button cancels" |
| 9.7 | Press any button other than Confirm (try Back, Up, Down, Left, Right) | Cancels back to the list; deck untouched |
| 9.8 | Repeat 9.6, press Confirm | Deck's directory (cards + metadata) and progress file are removed from the SD card; list reloads without it, selection/scroll clamped (deleting the last deck moves selection up; deleting the only deck shows "No decks found on SD card") |
| 9.9 | (Optional failure path, e.g. SD removed after the list loaded) | "Reset progress failed" / "Delete failed" appears at the bottom of the list, cleared on the next interaction; no crash |

**Report**:
- [ ] PASS - Reset and delete work; delete requires double-confirm; all cancel paths safe
- [ ] FAIL - Describe issue: _______________

### Test 10: Session Statistics

| Step | Action | Expected Result |
|------|--------|-----------------|
| 10.1 | Complete a review session | "Session Complete!" screen appears |
| 10.2 | Observe statistics | "Reviewed: N" and "Remaining: M" shown - Reviewed counts unique cards (a card that repeated via relearning counts once); Remaining counts cards still due today that were not rated this session (0 when everything due was rated); no percentage or timing stats |
| 10.3 | Press Confirm or Back | Returns to deck list |

**Report**:
- [ ] PASS - Stats display correctly
- [ ] FAIL - Describe issue: _______________

### Test 11: Exit to CrossPoint

| Step | Action | Expected Result |
|------|--------|-----------------|
| 11.1 | From main menu, select "Exit to CrossPoint" | No confirmation prompt; screen shows "Returning to CrossPoint..." |
| 11.2 | Wait for reboot | CrossPoint main firmware loads |
| 11.3 | Relaunch Flashink, open any screen (e.g. deck list), hold the Back button for ~1.5 seconds | Same "Returning to CrossPoint..." screen and reboot - long-press Back exits from any screen |

**Report**:
- [ ] PASS - Returns to CrossPoint successfully (both paths)
- [ ] FAIL - Describe issue: _______________

---

## Edge Case Tests

### Test 12: Empty Deck

| Step | Action | Expected Result |
|------|--------|-----------------|
| 12.1 | Upload an empty deck: `curl -F "deckId=empty-deck" -F "file=@empty.jsonl" http://192.168.4.1/upload-deck` (where `empty.jsonl` is a 0-byte file) | Request is rejected with HTTP 400 `{"error":"Empty or missing file"}`; no deck is created |
| 12.2 | Upload a one-card deck, then select it and rate the card Good | "Session Complete!" summary appears (Reviewed: 1, Remaining: 0) |
| 12.3 | Press Back (or Confirm) | Returns to deck list; the deck now shows "0 due" |
| 12.4 | Reopen the same deck | "No cards due today" with "Press Back to exit" (the card is scheduled for a future day) |

### Test 13: Large Text

| Step | Action | Expected Result |
|------|--------|-----------------|
| 13.1 | Create a card with very long text (500+ chars, no line breaks) | No crash; text word-wraps and paginates (full behavior in Test 7) |
| 13.2 | Create a card containing one unbroken 200+ character "word" | No crash; the word splits across lines at glyph boundaries instead of clipping at the screen edge |

### Test 14: Interrupted Session

| Step | Action | Expected Result |
|------|--------|-----------------|
| 14.1 | Start review, rate some cards | Progress being made |
| 14.2 | Power off device mid-session | Device powers off |
| 14.3 | Power on, return to Flashink, reopen the deck | Ratings made before power-off are in the progress file (progress is saved after every rating, including relearning re-ratings). With a working clock (X3, or X4 with the system time set) the already-rated cards are skipped and only the still-due ones are offered - including any cards last rated Again, which stayed due; with no clock (X4 after power-off without NTP) every card is offered again - see Test 5.8 |

### Test 15: Multiple Decks

| Step | Action | Expected Result |
|------|--------|-----------------|
| 15.1 | Upload 3+ decks with different `deckId` values (Test 2 flow) | All appear in the deck list, sorted by name |
| 15.2 | Study from different decks | Each deck tracks progress independently (one file per deck under `/.crosspoint/apps/flashink/progress/`) |

---

## Error Handling Tests

### Test 16: Invalid Upload Requests

Run these with curl against `http://192.168.4.1/upload-deck` while the
Upload Decks screen is open.

| Step | Action | Expected Result |
|------|--------|-----------------|
| 16.1 | Upload without a deckId: `curl -F "file=@cards.jsonl" http://192.168.4.1/upload-deck` | HTTP 400, `{"error":"Missing deckId parameter"}` |
| 16.2 | Upload with invalid characters: `curl -F "deckId=bad/id" -F "file=@cards.jsonl" ...` | HTTP 400, `{"error":"Invalid deckId characters"}` |
| 16.3 | Upload a file larger than 10MB | HTTP 413, `{"error":"File too large (max 10MB)"}` |
| 16.4 | In each case, check the device | "Decks uploaded" counter unchanged; no deck added; Day cutoff unchanged (tzMinutes only applies to successful uploads); no crash |
| 16.5 | On the browser upload page, select a .txt file in the file picker | Page shows "INVALID FILE TYPE. PLEASE USE .APKG"; button stays disabled |

### Test 17: WiFi Disconnect During Upload

| Step | Action | Expected Result |
|------|--------|-----------------|
| 17.1 | Start uploading a large (multi-MB) deck via curl | Upload in progress |
| 17.2 | Disconnect from the hotspot mid-upload | curl reports a connection error; device does not crash, "Decks uploaded" counter unchanged, deck not added to the list |
| 17.3 | Reconnect and retry the same upload | Upload succeeds; counter increments |
| 17.4 | On the browser page: click UPLOAD DECK, then drop off the hotspot before it finishes | Page shows "CONNECTION FAILED. CHECK YOU ARE ON THE DEVICE WIFI AND TRY AGAIN." in the error style; UPLOAD DECK is re-enabled for a retry |
| 17.5 | Rejoin the hotspot and click UPLOAD DECK again | Upload succeeds; success message and device counter increment |

---

## X3-Specific Tests

Run these on an Xteink X3. One `app.bin` serves both devices; these tests verify the X3 code paths.

### Test X3-1: Automatic Device Detection

| Step | Action | Expected Result |
|------|--------|-----------------|
| X3-1.1 | Boot the app on an X3 | Boot screen shows "Flashink X3" (serial log: "Flashink X3 Starting") |
| X3-1.2 | Reboot the app | Still detected as X3 (detection result is cached in NVS namespace `cphw`, key `dev_det`) |

**Note**: Detection runs an I2C probe on first boot only; later boots read the NVS cache. The NVS key `dev_ovr` in namespace `cphw` (0=auto, 1=force X4, 2=force X3) can override detection for debugging.

### Test X3-2: 792x528 Rendering

| Step | Action | Expected Result |
|------|--------|-----------------|
| X3-2.1 | Navigate all screens (menu, deck list, review, session complete, upload) | Content fills the panel; nothing cropped at the right or bottom edge |
| X3-2.2 | Observe header/selection bars | Full-width, no artifacts in the rightmost columns or bottom rows |

### Test X3-3: Tilt Gestures

| Step | Action | Expected Result |
|------|--------|-----------------|
| X3-3.1 | Main menu: tilt forward / back | Selection moves down / up |
| X3-3.2 | Deck list: tilt forward / back | Selection moves down / up |
| X3-3.3 | Review, single-page question shown: tilt forward | Answer revealed (tilt back does nothing) |
| X3-3.4 | Review, single-page answer shown: tilt forward / back | Card rated Good / Again |
| X3-3.5 | Review, MULTI-page question (Test 7 deck): tilt forward repeatedly | Pages forward one page per tilt; on the last page the next tilt forward reveals the answer. Tilt back pages backward; on page 1 it does nothing |
| X3-3.6 | Review, multi-page answer: tilt forward / back | Same paging; tilt forward on the last page rates Good, tilt back on page 1 rates Again |
| X3-3.7 | Review, rating bar shown: tilt forward / back | Rates Good / Again (no paging while the rating bar is open) |
| X3-3.8 | Session complete screen: tilt either way | Nothing happens, no gesture leaks into the next screen |
| X3-3.9 | Upload Decks screen: tilt either way | Nothing happens, no gesture leaks into the next screen |
| X3-3.10 | Deck options box open (Test 9): tilt either way | Nothing happens - tilt is ignored while the box is open |
| X3-3.11 | Upload screen SSID | Network name shown is "Flashink-X3" (an X4 shows "Flashink-X4") |

### Test X3-4: Tilt Toggle Persistence

| Step | Action | Expected Result |
|------|--------|-----------------|
| X3-4.1 | Main menu shows "Tilt: On" item (between "Upload Decks" and the "Day cutoff" item) | Item present on X3 only |
| X3-4.2 | Confirm on it | Label flips to "Tilt: Off"; tilt gestures stop working |
| X3-4.3 | Reboot the app | Still "Tilt: Off" (persisted to NVS); buttons unaffected |
| X3-4.4 | Toggle back to "Tilt: On" | Gestures work again |

### Test X3-5: X4 Regression Check

| Step | Action | Expected Result |
|------|--------|-----------------|
| X3-5.1 | Install the same `app.bin` on an X4 | Boot screen shows "Flashink X4"; 800x480 rendering unchanged |
| X3-5.2 | Main menu on X4 | No "Tilt" menu item; battery % and USB detection work as before |

---

## Performance Tests

### Test 18: Upload Performance

Time the curl upload from Test 2 with different JSONL sizes.

| Deck Size | Cards | Upload Time | Notes |
|-----------|-------|-------------|-------|
| Small | ~50 | _____ sec | |
| Medium | ~500 | _____ sec | |
| Large | ~2000 | _____ sec | |

### Test 19: Review Performance

| Metric | Result | Notes |
|--------|--------|-------|
| Time to display card | _____ ms | |
| Time to flip card | _____ ms | |
| Time to load next card | _____ ms | |
| E-ink refresh quality | [ ] Clean [ ] Ghosting | |

### Test 20: Memory Usage

| Scenario | Behavior | Notes |
|----------|----------|-------|
| Large deck (2000+ cards) | [ ] Works [ ] Slow [ ] Crash | |
| Multiple review sessions | [ ] Stable [ ] Memory leak | |

---

## UI/UX Feedback

### Display Quality

| Element | Rating (1-5) | Notes |
|---------|--------------|-------|
| Font readability | | |
| Button visibility | | |
| Contrast/clarity | | |
| Layout spacing | | |

### Navigation

| Aspect | Rating (1-5) | Notes |
|--------|--------------|-------|
| Menu intuitiveness | | |
| Button responsiveness | | |
| Back/cancel options | | |
| Error messages clarity | | |

### Overall Experience

| Question | Response |
|----------|----------|
| Would you use this app regularly? | |
| What's the #1 improvement needed? | |
| Any unexpected behaviors? | |
| Comparison to Anki mobile/desktop? | |

---

## Bug Report Template

If you find a bug, please report using this format:

```
### Bug Title
[Brief description]

### Steps to Reproduce
1. 
2. 
3. 

### Expected Behavior
[What should happen]

### Actual Behavior
[What actually happens]

### Environment
- Device model: [ ] X3 [ ] X4
- CrossPoint firmware version: 
- Flashink version: v0.2.0
- Deck used: 
- Battery level: 

### Screenshots/Photos
[If applicable]

### Additional Context
[Any other relevant info]
```

---

## Test Session Summary

**Tester**: _________________
**Date**: _________________
**Device Serial** (optional): _________________

### Results Summary

| Test | Status | Notes |
|------|--------|-------|
| 1. Main Menu | [ ] Pass [ ] Fail | |
| 2. Deck Upload | [ ] Pass [ ] Fail | |
| 3. Deck List | [ ] Pass [ ] Fail | |
| 4. Review Session | [ ] Pass [ ] Fail | |
| 5. SM-2 Scheduling + Due Dates | [ ] Pass [ ] Fail | |
| 6. In-Session Relearning | [ ] Pass [ ] Fail | |
| 7. Long-Card Paging | [ ] Pass [ ] Fail | |
| 8. Day Cutoff | [ ] Pass [ ] Fail | |
| 9. Deck Management | [ ] Pass [ ] Fail | |
| 10. Session Stats | [ ] Pass [ ] Fail | |
| 11. Exit to CrossPoint | [ ] Pass [ ] Fail | |
| 12. Empty Deck | [ ] Pass [ ] Fail | |
| 13. Large Text | [ ] Pass [ ] Fail | |
| 14. Interrupted Session | [ ] Pass [ ] Fail | |
| 15. Multiple Decks | [ ] Pass [ ] Fail | |
| 16. Invalid Upload | [ ] Pass [ ] Fail | |
| 17. WiFi Disconnect | [ ] Pass [ ] Fail | |
| X3-1. Device Detection | [ ] Pass [ ] Fail [ ] N/A | |
| X3-2. 792x528 Rendering | [ ] Pass [ ] Fail [ ] N/A | |
| X3-3. Tilt Gestures | [ ] Pass [ ] Fail [ ] N/A | |
| X3-4. Tilt Persistence | [ ] Pass [ ] Fail [ ] N/A | |
| X3-5. X4 Regression | [ ] Pass [ ] Fail [ ] N/A | |

### Overall Assessment

- [ ] Ready for daily use
- [ ] Usable with minor issues
- [ ] Major issues need fixing
- [ ] Not functional

### Top 3 Issues Found
1. 
2. 
3. 

### Top 3 Positive Observations
1. 
2. 
3. 

---

## Submitting Feedback

Please submit your completed test results via:

1. **GitHub Issues**: https://github.com/DChells/crosspoint-x4-anki/issues
2. **Direct message** to maintainer

Thank you for testing!
