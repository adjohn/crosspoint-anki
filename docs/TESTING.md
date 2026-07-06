# Anki Testing Guide

This document provides comprehensive testing instructions for the Anki flashcard app on CrossPoint devices (Xteink X3 and X4). The same `app.bin` runs on both devices; unless a step says otherwise, run the tests on whichever device you have. X3-specific tests are in [X3-Specific Tests](#x3-specific-tests).

---

## Prerequisites

- Xteink X3 or X4 device with CrossPoint firmware 0.16.0+ (PR #679 app extension support)
- A computer or phone with WiFi (deck uploads use a hotspot the device creates itself; a shared WiFi network is only needed if you install the app via CrossPoint File Transfer in STA mode)
- Sample deck in JSONL format - see [Test Decks](#test-decks) below
- Battery > 20% (required for app installation)

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

### Known Limitations (v0.1.0)
- **Browser .apkg upload not wired yet**: the upload page loads and lets you
  select an .apkg file, but its UPLOAD DECK button is a stub - uploads must
  go through the HTTP API (`POST /upload-deck`) with a JSONL file for now
- **Max upload size**: 10MB
- **Text only**: No images or audio
- **English/Latin-1**: Unicode characters may not render correctly
- **No cloze deletions**: Basic cards only
- **No due-date scheduling**: every session reviews all cards in the deck;
  SM-2 ease/interval/repetitions are tracked in the progress file but due
  dates are not yet enforced

---

## Installation Test

### 1. Download the Release

```
https://github.com/DChells/crosspoint-x4-anki/releases/tag/v0.1.0
```

Download: `anki-v0.1.0.zip`

### 2. Upload via CrossPoint File Transfer

| Step | Action | Expected Result |
|------|--------|-----------------|
| 2.1 | On the device: Home → File Transfer | File Transfer screen appears |
| 2.2 | Connect to WiFi (STA) or create hotspot (AP) | IP address displayed on screen |
| 2.3 | On computer: open the URL shown | CrossPoint web interface loads |
| 2.4 | Click **Apps** tab | Apps management page opens |
| 2.5 | Upload `anki-v0.1.0.zip` | Upload progress shown, success message |

**Verify**: App appears in the list with name "Anki"

### 3. Install the App

| Step | Action | Expected Result |
|------|--------|-----------------|
| 3.1 | On the device: Home → Apps | Apps list shows "Anki" |
| 3.2 | Select "Anki" | App details screen |
| 3.3 | Press Install | Progress indicator, device reboots |
| 3.4 | Wait for reboot | Anki app main menu appears |

**Checkpoint**: If installation fails, note:
- Battery level
- Error message (if any)
- Device behavior

---

## App Functionality Tests

### Test 1: Main Menu Navigation

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1.1 | App boots | "CrossPoint Anki" header, menu items visible |
| 1.2 | Observe menu items | X4: "Study", "Upload Decks", "Exit to CrossPoint". X3 adds "Tilt: On" (or "Tilt: Off") between "Upload Decks" and "Exit to CrossPoint" |
| 1.3 | Press side Up/Down buttons | Highlight bar moves between options |

**Report**:
- [ ] PASS - Menu displays correctly
- [ ] FAIL - Describe issue: _______________

### Test 2: Deck Upload (WiFi)

| Step | Action | Expected Result |
|------|--------|-----------------|
| 2.1 | Select "Upload Decks" | Instruction screen: network **Anki-X3** (X3) or **Anki-X4** (X4), password **ankideck123**, `http://192.168.4.1` (plus "or http://anki.local" if mDNS started), "Decks uploaded: 0", "Press Back when done", and a QR code on the right captioned "Scan to join WiFi" |
| 2.2 | On a phone: scan the QR code | Phone joins the Anki-X3/Anki-X4 hotspot without typing the password |
| 2.3 | On computer: join the hotspot manually and open `http://192.168.4.1` | Upload page ("Upload Anki Deck") loads; `/` redirects to `/upload.html` |
| 2.4 | Upload the test deck via the HTTP API: `curl -F "deckId=test-deck" -F "name=Test Deck" -F "file=@cards.jsonl" http://192.168.4.1/upload-deck` | JSON response `{"success":true, "bytes":..., "cards":5, "deckId":"test-deck"}` |
| 2.5 | Watch the device screen (updates within ~1 second) | "Decks uploaded: 1" |
| 2.6 | (Optional) Leave the upload screen idle for over 5 minutes | Device does NOT auto-sleep (the upload screen suppresses the 5-minute sleep timer so the hotspot survives); on other screens 5 idle minutes shows "Sleeping..." and sleeps |
| 2.7 | Press Back, then select "Study" | Hotspot shuts down; "Test Deck" visible in deck list with "5 cards" |

**Note**: the browser page's UPLOAD DECK button is a stub in v0.1.0 (it only
prints "INITIALIZING UPLOAD..."); the curl API is the supported upload path.

**Report**:
- [ ] PASS - Deck uploads and appears in list
- [ ] FAIL - Describe issue: _______________
- Upload time for test deck: _______ seconds
- Deck name displayed correctly: [ ] Yes [ ] No

### Test 3: Deck List

| Step | Action | Expected Result |
|------|--------|-----------------|
| 3.1 | Select "Study" from main menu | "Select Deck" list appears |
| 3.2 | Observe deck info | Deck name on the left, card count ("N cards") on the right |
| 3.3 | Navigate between decks (if multiple) | Selection moves correctly; list scrolls with a scrollbar when it overflows |
| 3.4 | Press Confirm on a deck | Review session starts (card front shown) |
| 3.5 | Press Back from the deck list | Returns to main menu |

**Report**:
- [ ] PASS - Deck list works correctly
- [ ] FAIL - Describe issue: _______________

### Test 4: Review Session (Core Functionality)

| Step | Action | Expected Result |
|------|--------|-----------------|
| 4.1 | Start review | Card front (question) displayed, hint "Press Confirm to reveal" |
| 4.2 | Press Confirm | Card back (answer) displayed below the front, hint "Press Confirm to rate" |
| 4.3 | Press Confirm again | Rating bar at the bottom: "Again", "Hard", "Good", "Easy" |
| 4.4 | Rate card as "Good" (press side Up button) | Next card's front appears |
| 4.5 | Try the other ratings on later cards (Left=Again, Down=Hard, Right=Easy) | Each rating advances to the next card |
| 4.6 | Rate the last card | Session complete screen appears automatically |
| 4.7 | Press Back mid-session (on a later run) | Returns to deck list; ratings so far are kept |

**Report**:
- [ ] PASS - Review flow works correctly
- [ ] FAIL - Describe issue: _______________

**Text rendering**:
- [ ] Text readable and properly sized
- [ ] Long text wraps correctly
- [ ] Special characters display correctly (if applicable)

### Test 5: SM-2 Scheduling

This tests spaced repetition logic. Intervals are not shown on screen; verify
them in the progress file on the SD card
(`/.crosspoint/apps/anki/progress/<deckId>.json`).

| Step | Action | Expected Result |
|------|--------|-----------------|
| 5.1 | Review a new card, rate "Good" | Progress file: `repetitions` = 1, `interval` = 1, `ease` ≈ 2.5 |
| 5.2 | Review the same card in a later session, rate "Good" | `repetitions` = 2, `interval` increases |
| 5.3 | Rate a card "Again" | Its `repetitions` resets to 0, `ease` drops |
| 5.4 | Rate a card "Easy" | Its `ease` increases above cards rated "Good" |

**Note**: v0.1.0 does not enforce due dates - every session reviews all
cards, and the `due` field in the progress file stays empty. Only verify the
ease/interval/repetitions bookkeeping.

**Report**:
- [ ] PASS - Progress file values change as described
- [ ] FAIL - Describe issue: _______________

### Test 6: Session Statistics

| Step | Action | Expected Result |
|------|--------|-----------------|
| 6.1 | Complete a review session | "Session Complete!" screen appears |
| 6.2 | Observe statistics | "Reviewed: N" and "Remaining: N" shown (Remaining is 0 when the whole deck was rated); no percentage or timing stats in v0.1.0 |
| 6.3 | Press Confirm or Back | Returns to deck list |

**Report**:
- [ ] PASS - Stats display correctly
- [ ] FAIL - Describe issue: _______________

### Test 7: Exit to CrossPoint

| Step | Action | Expected Result |
|------|--------|-----------------|
| 7.1 | From main menu, select "Exit to CrossPoint" | No confirmation prompt; screen shows "Returning to CrossPoint..." |
| 7.2 | Wait for reboot | CrossPoint main firmware loads |
| 7.3 | Relaunch Anki, open any screen (e.g. deck list), hold the Back button for ~1.5 seconds | Same "Returning to CrossPoint..." screen and reboot - long-press Back exits from any screen |

**Report**:
- [ ] PASS - Returns to CrossPoint successfully (both paths)
- [ ] FAIL - Describe issue: _______________

---

## Edge Case Tests

### Test 8: Empty Deck

| Step | Action | Expected Result |
|------|--------|-----------------|
| 8.1 | Upload an empty deck: `curl -F "deckId=empty-deck" -F "file=@empty.jsonl" http://192.168.4.1/upload-deck` (where `empty.jsonl` is a 0-byte file) | Request is rejected with HTTP 400 `{"error":"Empty or missing file"}`; no deck is created |
| 8.2 | Upload a one-card deck, then select it and rate the card | After the last card, "Deck Complete!" screen with "Press Back to exit" |
| 8.3 | Press Back (or Confirm) | Returns to deck list |

### Test 9: Large Text

| Step | Action | Expected Result |
|------|--------|-----------------|
| 9.1 | Create card with very long text (500+ chars) | No crash; text renders as far as it fits (v0.1.0 has no wrapping or scrolling - note how it clips) |

### Test 10: Interrupted Session

| Step | Action | Expected Result |
|------|--------|-----------------|
| 10.1 | Start review, rate some cards | Progress being made |
| 10.2 | Power off device mid-session | Device powers off |
| 10.3 | Power on, return to Anki app, reopen the deck | Ratings made before power-off are in the progress file (progress is saved after every rating); the session itself restarts from the first card |

### Test 11: Multiple Decks

| Step | Action | Expected Result |
|------|--------|-----------------|
| 11.1 | Upload 3+ decks with different `deckId` values (Test 2 flow) | All appear in the deck list, sorted by name |
| 11.2 | Study from different decks | Each deck tracks progress independently (one file per deck under `/.crosspoint/apps/anki/progress/`) |

---

## Error Handling Tests

### Test 12: Invalid Upload Requests

Run these with curl against `http://192.168.4.1/upload-deck` while the
Upload Decks screen is open.

| Step | Action | Expected Result |
|------|--------|-----------------|
| 12.1 | Upload without a deckId: `curl -F "file=@cards.jsonl" http://192.168.4.1/upload-deck` | HTTP 400, `{"error":"Missing deckId parameter"}` |
| 12.2 | Upload with invalid characters: `curl -F "deckId=bad/id" -F "file=@cards.jsonl" ...` | HTTP 400, `{"error":"Invalid deckId characters"}` |
| 12.3 | Upload a file larger than 10MB | HTTP 413, `{"error":"File too large (max 10MB)"}` |
| 12.4 | In each case, check the device | "Decks uploaded" counter unchanged; no deck added; no crash |
| 12.5 | On the browser upload page, select a .txt file in the file picker | Page shows "INVALID FILE TYPE. PLEASE USE .APKG"; button stays disabled |

### Test 13: WiFi Disconnect During Upload

| Step | Action | Expected Result |
|------|--------|-----------------|
| 13.1 | Start uploading a large (multi-MB) deck via curl | Upload in progress |
| 13.2 | Disconnect from the hotspot mid-upload | curl reports a connection error; device does not crash, "Decks uploaded" counter unchanged, deck not added to the list |
| 13.3 | Reconnect and retry the same upload | Upload succeeds; counter increments |

---

## X3-Specific Tests

Run these on an Xteink X3. One `app.bin` serves both devices; these tests verify the X3 code paths.

### Test X3-1: Automatic Device Detection

| Step | Action | Expected Result |
|------|--------|-----------------|
| X3-1.1 | Boot the app on an X3 | Boot screen shows "Anki X3" (serial log: "Anki X3 Starting") |
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
| X3-3.3 | Review, question shown: tilt forward | Answer revealed (tilt back does nothing) |
| X3-3.4 | Review, answer shown: tilt forward / back | Card rated Good / Again |
| X3-3.5 | Session complete screen: tilt either way | Nothing happens, no gesture leaks into the next screen |
| X3-3.6 | Upload Decks screen: tilt either way | Nothing happens, no gesture leaks into the next screen |
| X3-3.7 | Upload screen SSID | Network name shown is "Anki-X3" (an X4 shows "Anki-X4") |

### Test X3-4: Tilt Toggle Persistence

| Step | Action | Expected Result |
|------|--------|-----------------|
| X3-4.1 | Main menu shows "Tilt: On" item (between "Upload Decks" and "Exit to CrossPoint") | Item present on X3 only |
| X3-4.2 | Confirm on it | Label flips to "Tilt: Off"; tilt gestures stop working |
| X3-4.3 | Reboot the app | Still "Tilt: Off" (persisted to NVS); buttons unaffected |
| X3-4.4 | Toggle back to "Tilt: On" | Gestures work again |

### Test X3-5: X4 Regression Check

| Step | Action | Expected Result |
|------|--------|-----------------|
| X3-5.1 | Install the same `app.bin` on an X4 | Boot screen shows "Anki X4"; 800x480 rendering unchanged |
| X3-5.2 | Main menu on X4 | No "Tilt" menu item; battery % and USB detection work as before |

---

## Performance Tests

### Test 14: Upload Performance

Time the curl upload from Test 2 with different JSONL sizes.

| Deck Size | Cards | Upload Time | Notes |
|-----------|-------|-------------|-------|
| Small | ~50 | _____ sec | |
| Medium | ~500 | _____ sec | |
| Large | ~2000 | _____ sec | |

### Test 15: Review Performance

| Metric | Result | Notes |
|--------|--------|-------|
| Time to display card | _____ ms | |
| Time to flip card | _____ ms | |
| Time to load next card | _____ ms | |
| E-ink refresh quality | [ ] Clean [ ] Ghosting | |

### Test 16: Memory Usage

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
- Anki app version: v0.1.0
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
| 5. SM-2 Scheduling | [ ] Pass [ ] Fail | |
| 6. Session Stats | [ ] Pass [ ] Fail | |
| 7. Exit to CrossPoint | [ ] Pass [ ] Fail | |
| 8. Empty Deck | [ ] Pass [ ] Fail | |
| 9. Large Text | [ ] Pass [ ] Fail | |
| 10. Interrupted Session | [ ] Pass [ ] Fail | |
| 11. Multiple Decks | [ ] Pass [ ] Fail | |
| 12. Invalid Upload | [ ] Pass [ ] Fail | |
| 13. WiFi Disconnect | [ ] Pass [ ] Fail | |
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
