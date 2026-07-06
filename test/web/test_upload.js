#!/usr/bin/env node
// End-to-end driver: loads upload.html from the mock device server in headless
// Chromium, uploads the fixture .apkg, and asserts both the UI outcome and the
// exact JSONL bytes the "device" received.
//
// Usage: node test_upload.js PORT BUILD_DIR

const fs = require('fs');
const path = require('path');
const { chromium } = require('playwright');

const port = process.argv[2];
const buildDir = process.argv[3];
if (!port || !buildDir) {
    console.error('usage: node test_upload.js PORT BUILD_DIR');
    process.exit(2);
}

const fixturePath = path.join(buildDir, 'test-deck.apkg');
const expected = JSON.parse(fs.readFileSync(path.join(buildDir, 'expected.json'), 'utf8'));
const receivedDir = path.join(buildDir, 'received');

let checks = 0;
let failures = 0;

function assert(cond, msg) {
    checks++;
    if (cond) {
        console.log('  ok   ' + msg);
    } else {
        failures++;
        console.log('  FAIL ' + msg);
    }
}

function assertEqual(actual, expectedVal, msg) {
    const a = JSON.stringify(actual);
    const e = JSON.stringify(expectedVal);
    assert(a === e, msg + (a === e ? '' : `\n       expected: ${e}\n       actual:   ${a}`));
}

(async () => {
    const browser = await chromium.launch({ headless: true });
    const page = await browser.newPage();

    const pageErrors = [];
    page.on('pageerror', err => pageErrors.push(String(err)));
    page.on('console', msg => {
        if (msg.type() === 'error') pageErrors.push('console.error: ' + msg.text());
    });

    console.log('-- loading page');
    await page.goto(`http://127.0.0.1:${port}/upload.html`, { waitUntil: 'load' });
    assertEqual(await page.title(), 'Anki Deck Upload', 'page title');
    assert(await page.locator('#uploadBtn').isDisabled(), 'upload button disabled before file select');

    console.log('-- selecting fixture and uploading');
    await page.setInputFiles('#fileInput', fixturePath);
    assert(!(await page.locator('#uploadBtn').isDisabled()), 'upload button enabled after file select');
    await page.click('#uploadBtn');

    await page.waitForSelector('#status .success, #status .error', { timeout: 60000 });
    const statusText = await page.locator('#status').innerText();
    const isSuccess = (await page.locator('#status .success').count()) === 1;
    assert(isSuccess, 'success message shown (got: ' + statusText.trim() + ')');
    assert(statusText.includes('5 CARDS'), 'success message reports 5 cards');
    assert(statusText.includes('test-deck'), 'success message names the deck');
    assert(!(await page.locator('#uploadBtn').isDisabled()), 'upload button re-enabled after upload');
    const progressValue = await page.locator('progress').evaluate(el => el.value);
    assertEqual(progressValue, 100, 'progress bar at 100');

    assertEqual(pageErrors, [], 'no page/console errors');

    console.log('-- verifying params received by mock device');
    const params = JSON.parse(fs.readFileSync(path.join(receivedDir, 'params.json'), 'utf8'));
    assertEqual(params.deckId, expected.params.deckId, 'deckId param');
    assertEqual(params.name, expected.params.name, 'name param');
    assertEqual(params.cardCount, expected.params.cardCount, 'cardCount param');

    console.log('-- verifying JSONL received by mock device');
    const jsonlPath = path.join(receivedDir, expected.params.deckId + '.jsonl');
    const raw = fs.readFileSync(jsonlPath, 'utf8');
    assert(raw.endsWith('\n'), 'JSONL ends with trailing newline');
    const lines = raw.split('\n').filter(l => l.length > 0);
    assertEqual(lines.length, expected.cards.length, `JSONL has ${expected.cards.length} lines`);

    for (let i = 0; i < Math.min(lines.length, expected.cards.length); i++) {
        let card;
        try {
            card = JSON.parse(lines[i]);
        } catch (e) {
            assert(false, `line ${i + 1} parses as JSON`);
            continue;
        }
        assertEqual(Object.keys(card).sort(), ['back', 'front', 'id', 'tags'],
                    `line ${i + 1} has exactly id/front/back/tags`);
        const exp = expected.cards[i];
        assertEqual(card.id, exp.id, `card ${i + 1} id`);
        assertEqual(card.front, exp.front, `card ${i + 1} front`);
        assertEqual(card.back, exp.back, `card ${i + 1} back`);
        assertEqual(card.tags, exp.tags, `card ${i + 1} tags`);
    }

    await browser.close();

    console.log(`\n${checks} checks, ${failures} failures`);
    process.exit(failures === 0 ? 0 : 1);
})().catch(err => {
    console.error('driver error:', err);
    process.exit(1);
});
