#!/usr/bin/env python3
"""Build a minimal but valid .apkg fixture matching what web/js/apkg-parser.js
SELECTs (notes: id, flds, tags; cards: id, nid, ord, type, queue, due, ivl,
factor, reps, lapses), plus expected.json describing the exact JSONL the
browser should produce for it."""
import json
import os
import sqlite3
import sys
import zipfile

US = "\x1f"  # Anki field separator

NOTES = [
    # (id, flds, tags)
    (1001, "What is the capital of France?" + US + "Paris", " geography europe "),
    (1002,
     "<div>What is <b>2 + 2</b>?</div>" + US +
     "<div>4</div><br><i>basic&nbsp;math</i> &amp; counting [sound:foo.mp3]",
     "math"),
    (1003, "日本語で「猫」は何ですか？" + US + "ねこ (neko) — 🐱", ""),
    (1004, "Hello" + US + "Bonjour", "language"),
]

CARDS = [
    # (id, nid, ord, type, queue, due, ivl, factor, reps, lapses)
    (2001, 1001, 0, 0, 0, 1, 0, 2500, 0, 0),
    (2002, 1002, 0, 0, 0, 2, 0, 2500, 0, 0),
    (2003, 1003, 0, 0, 0, 3, 0, 2500, 0, 0),
    (2004, 1004, 0, 0, 0, 4, 0, 2500, 0, 0),
    (2005, 1004, 1, 0, 0, 5, 0, 2500, 0, 0),  # reversed card: ord picks field 1
]

# What apkg-parser.js must emit for the fixture above (HTML stripped, tags
# trimmed/split, ord selecting the front field with the next field as back).
EXPECTED_CARDS = [
    {"id": "2001", "front": "What is the capital of France?", "back": "Paris",
     "tags": ["geography", "europe"]},
    {"id": "2002", "front": "What is 2 + 2?",
     "back": "4\n\nbasic math & counting", "tags": ["math"]},
    {"id": "2003", "front": "日本語で「猫」は何ですか？",
     "back": "ねこ (neko) — 🐱", "tags": []},
    {"id": "2004", "front": "Hello", "back": "Bonjour", "tags": ["language"]},
    {"id": "2005", "front": "Bonjour", "back": "Hello", "tags": ["language"]},
]

EXPECTED_PARAMS = {"deckId": "test-deck", "name": "test-deck", "cardCount": "5"}


def build_db(path):
    if os.path.exists(path):
        os.remove(path)
    conn = sqlite3.connect(path)
    cur = conn.cursor()
    cur.execute("""
        CREATE TABLE notes (
            id INTEGER PRIMARY KEY, guid TEXT, mid INTEGER, mod INTEGER,
            usn INTEGER, tags TEXT, flds TEXT, sfld TEXT, csum INTEGER,
            flags INTEGER, data TEXT
        )""")
    cur.execute("""
        CREATE TABLE cards (
            id INTEGER PRIMARY KEY, nid INTEGER, did INTEGER, ord INTEGER,
            mod INTEGER, usn INTEGER, type INTEGER, queue INTEGER,
            due INTEGER, ivl INTEGER, factor INTEGER, reps INTEGER,
            lapses INTEGER, left INTEGER, odue INTEGER, odid INTEGER,
            flags INTEGER, data TEXT
        )""")
    for nid, flds, tags in NOTES:
        cur.execute(
            "INSERT INTO notes VALUES (?, ?, 1, 0, 0, ?, ?, '', 0, 0, '')",
            (nid, "guid%d" % nid, tags, flds))
    for cid, nid, ord_, typ, queue, due, ivl, factor, reps, lapses in CARDS:
        cur.execute(
            "INSERT INTO cards VALUES (?, ?, 1, ?, 0, 0, ?, ?, ?, ?, ?, ?, ?, 0, 0, 0, 0, '')",
            (cid, nid, ord_, typ, queue, due, ivl, factor, reps, lapses))
    conn.commit()
    conn.close()


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.abspath(__file__))
    os.makedirs(out_dir, exist_ok=True)

    db_path = os.path.join(out_dir, "collection.anki2")
    build_db(db_path)

    apkg_path = os.path.join(out_dir, "test-deck.apkg")
    with zipfile.ZipFile(apkg_path, "w", zipfile.ZIP_DEFLATED) as zf:
        zf.write(db_path, "collection.anki2")
        zf.writestr("media", "{}")
    os.remove(db_path)

    expected_path = os.path.join(out_dir, "expected.json")
    with open(expected_path, "w", encoding="utf-8") as f:
        json.dump({"cards": EXPECTED_CARDS, "params": EXPECTED_PARAMS},
                  f, ensure_ascii=False, indent=2)

    print("fixture: %s" % apkg_path)
    print("expected: %s" % expected_path)


if __name__ == "__main__":
    main()
