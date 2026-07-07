// apkg-parser.js

// Load dependencies from local paths
if (typeof importScripts === 'function') {
    importScripts('../lib/jszip.min.js');
    importScripts('../lib/sql-wasm.js');
}

class ApkgParser {
    constructor() {
        this.SQL = null;
        this.initPromise = this.initSqlJs();
    }
    
    async initSqlJs() {
        // sql.js initialization
        const pathPrefix = (typeof importScripts === 'function') ? '../lib/' : 'lib/';

        this.SQL = await initSqlJs({
            locateFile: file => `${pathPrefix}${file}`
        });
    }
    
    async parseApkg(file, onProgress) {
        await this.initPromise;
        
        // 1. Read file as ArrayBuffer
        const arrayBuffer = await file.arrayBuffer();
        
        // 2. Extract ZIP
        const zip = await JSZip.loadAsync(arrayBuffer);
        
        // 3. Find SQLite database
        const dbFile = zip.file('collection.anki21') || zip.file('collection.anki2');
        if (!dbFile) {
            throw new Error('No Anki database found in .apkg');
        }
        
        // 4. Extract database
        const dbData = await dbFile.async('uint8array');
        
        // 5. Open with sql.js
        const db = new this.SQL.Database(dbData);
        
        // 6. Query notes table
        const notesResult = db.exec(`
            SELECT id, flds, tags FROM notes
        `);
        
        // 7. Query cards table (for card data)
        const cardsResult = db.exec(`
            SELECT id, nid, ord, type, queue, due, ivl, factor, reps, lapses 
            FROM cards
        `);
        
        // 8. Build deck structure
        const deck = {
            id: this.generateDeckId(file.name),
            name: file.name.replace(/\.apkg$/i, ''),
            cardCount: 0,
            cards: []
        };

        // 9. Index notes by id (fields split by 0x1f unit separator)
        const notes = new Map();
        if (notesResult.length > 0) {
            for (const row of notesResult[0].values) {
                const [id, flds, tags] = row;
                notes.set(id, {
                    fields: String(flds ?? '').split('\x1f'),
                    tags: tags ? String(tags).trim().split(/\s+/).filter(t => t.length > 0) : []
                });
            }
        }

        // 10. One output card per Anki card; ord picks which field is the
        // front (Basic = ord 0 -> field 0, Basic reversed = ord 1 -> field 1),
        // the next field (wrapping) is the back.
        const cardRows = cardsResult.length > 0 ? cardsResult[0].values : [];
        for (let i = 0; i < cardRows.length; i++) {
            const row = cardRows[i];
            const cardId = row[0];
            const nid = row[1];
            const ord = row[2];

            const note = notes.get(nid);
            if (!note || note.fields.length === 0) {
                continue;
            }

            const n = note.fields.length;
            const frontIdx = (typeof ord === 'number' && ord >= 0 && ord < n) ? ord : 0;
            const backIdx = (frontIdx + 1) % n;

            deck.cards.push({
                id: cardId.toString(),
                front: this.stripHtml(note.fields[frontIdx] || ''),
                back: n > 1 ? this.stripHtml(note.fields[backIdx] || '') : '',
                tags: note.tags
            });

            if (onProgress) {
                onProgress(i + 1, cardRows.length);
            }
        }

        deck.cardCount = deck.cards.length;

        // 11. Close database
        db.close();

        return deck;
    }

    // Reduce Anki HTML field content to plain text suitable for e-ink display.
    stripHtml(html) {
        return String(html)
            .replace(/\[sound:[^\]]*\]/gi, '')
            .replace(/<br\s*\/?>/gi, '\n')
            .replace(/<\/(div|p|li|ul|ol|tr|table|h[1-6]|blockquote)>/gi, '\n')
            .replace(/<[^>]*>/g, '')
            .replace(/&nbsp;/gi, ' ')
            .replace(/&lt;/gi, '<')
            .replace(/&gt;/gi, '>')
            .replace(/&quot;/gi, '"')
            .replace(/&#(\d+);/g, (m, d) => {
                const cp = parseInt(d, 10);
                return cp <= 0x10FFFF ? String.fromCodePoint(cp) : m;
            })
            .replace(/&#x([0-9a-f]+);/gi, (m, h) => {
                const cp = parseInt(h, 16);
                return cp <= 0x10FFFF ? String.fromCodePoint(cp) : m;
            })
            .replace(/&amp;/gi, '&')
            .replace(/[ \t]+/g, ' ')
            .replace(/ *\n */g, '\n')
            .replace(/\n{3,}/g, '\n\n')
            .trim();
    }

    generateDeckId(filename) {
        const slug = filename
            .toLowerCase()
            .replace(/\.apkg$/, '')
            .replace(/[^a-z0-9]+/g, '-')
            .replace(/^-+|-+$/g, '')
            .slice(0, 64)
            .replace(/-+$/, '');
        return slug || 'deck';
    }
}

// Export for use in upload.html
if (typeof module !== 'undefined' && module.exports) {
    module.exports = ApkgParser;
}
