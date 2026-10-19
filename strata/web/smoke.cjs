// Smoke test for the browser build of mb_strata (strata/web/shell.html), in
// headless Chromium through Playwright:
//
//   node strata/web/smoke.cjs <site url> <out dir> [--phone] [--expect-state HEX]
//
// 1. ?bench=2000 runs 2,000 ticks of the default chamber as WebAssembly and
//    prints the state hash; --expect-state makes a different hash a failure.
// 2. The page itself: the chamber must draw (not a blank canvas), ticks must
//    advance, and frames per second are measured over five seconds.
// 3. The controls: the Pause button stops the clock; a drag pans and a wheel
//    (or, with --phone, a one-finger drag and a two-finger pinch) changes the
//    picture. Screenshots go to <out dir>.
//
// Needs `playwright` (1.61.1, which pins Chromium) where Node can require it.
'use strict';
const fs = require('fs');
const path = require('path');
const { chromium, devices } = require('playwright');

const argv = process.argv.slice(2);
const url = argv[0];
const outDir = argv[1];
const phone = argv.includes('--phone');
const expectIdx = argv.indexOf('--expect-state');
const expectState = expectIdx >= 0 ? argv[expectIdx + 1] : null;
if (!url || !outDir) {
    console.error('usage: node smoke.cjs <site url> <out dir> [--phone] [--expect-state HEX]');
    process.exit(2);
}
fs.mkdirSync(outDir, { recursive: true });
const failures = [];
const check = (ok, what) => { console.log(`${ok ? 'ok  ' : 'FAIL'} ${what}`); if (!ok) failures.push(what); };

// Distinct colours and mean brightness of a PNG, decoded in the page.
async function imageStats(page, png) {
    return page.evaluate(async (b64) => {
        const img = new Image();
        img.src = 'data:image/png;base64,' + b64;
        await img.decode();
        const c = document.createElement('canvas');
        c.width = img.width; c.height = img.height;
        const g = c.getContext('2d');
        g.drawImage(img, 0, 0);
        const d = g.getImageData(0, 0, c.width, c.height).data;
        const colours = new Set();
        let sum = 0;
        for (let i = 0; i < d.length; i += 4) {
            sum += d[i] + d[i + 1] + d[i + 2];
            if (colours.size < 5000) colours.add((d[i] << 16) | (d[i + 1] << 8) | d[i + 2]);
        }
        return { colours: colours.size, mean: sum / (d.length / 4) / 3 };
    }, png.toString('base64'));
}

// Fraction of pixels that differ between two same-sized PNGs.
async function diffFraction(page, a, b) {
    return page.evaluate(async ([a64, b64]) => {
        const load = async (s) => { const i = new Image(); i.src = 'data:image/png;base64,' + s; await i.decode(); return i; };
        const [ia, ib] = [await load(a64), await load(b64)];
        const px = (img) => {
            const c = document.createElement('canvas');
            c.width = img.width; c.height = img.height;
            const g = c.getContext('2d');
            g.drawImage(img, 0, 0);
            return g.getImageData(0, 0, c.width, c.height).data;
        };
        const [da, db] = [px(ia), px(ib)];
        let n = 0;
        for (let i = 0; i < da.length; i += 4) {
            if (Math.abs(da[i] - db[i]) + Math.abs(da[i + 1] - db[i + 1]) + Math.abs(da[i + 2] - db[i + 2]) > 24) n++;
        }
        return n / (da.length / 4);
    }, [a.toString('base64'), b.toString('base64')]);
}

async function touchDrag(cdp, points, steps = 12) {
    // points: [{from:[x,y], to:[x,y]}, ...] one per finger, in CSS pixels.
    const at = (t) => points.map((p, id) => ({
        x: p.from[0] + (p.to[0] - p.from[0]) * t, y: p.from[1] + (p.to[1] - p.from[1]) * t, id,
    }));
    await cdp.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: at(0) });
    for (let s = 1; s <= steps; ++s) {
        await cdp.send('Input.dispatchTouchEvent', { type: 'touchMove', touchPoints: at(s / steps) });
    }
    await cdp.send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
}

(async () => {
    const browser = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
    const contextOptions = phone
        ? { ...devices['iPhone 13'], defaultBrowserType: undefined }
        : { viewport: { width: 1280, height: 900 }, deviceScaleFactor: 1 };
    delete contextOptions.defaultBrowserType;
    const context = await browser.newContext(contextOptions);
    const page = await context.newPage();
    const errors = [];
    page.on('pageerror', (e) => errors.push(String(e)));
    page.on('console', (m) => { if (m.type() === 'error') errors.push(m.text()); });
    const sep = url.includes('?') ? '&' : '?';

    // 1. Determinism as WebAssembly.
    await page.goto(url + sep + 'bench=2000', { waitUntil: 'commit' });
    await page.waitForFunction(() => (window.mbLog || []).some((l) => l.startsWith('bench:')), null, { timeout: 120000 });
    const benchLine = (await page.evaluate(() => window.mbLog)).find((l) => l.startsWith('bench:'));
    console.log(benchLine);
    const state = (benchLine.match(/state ([0-9a-f]{16})/) || [])[1];
    console.log(`wasm state after 2000 ticks: ${state}`);
    fs.writeFileSync(path.join(outDir, 'wasm-state.txt'), `${state}\n${benchLine}\n`);
    if (expectState) check(state === expectState, `wasm state ${state} equals the native state ${expectState}`);

    // 2. The page.
    await page.goto(url, { waitUntil: 'commit' });
    await page.waitForFunction(() => window.mbReady === true, null, { timeout: 120000 });
    await page.waitForTimeout(4000);
    // SwiftShader draws a frame in a second or more on a busy machine: shoot
    // the stage's area of the page rather than wait for the element to settle.
    const box = await page.locator('#stage').boundingBox();
    // Waits until two more frames have been drawn since an input.
    const settle = async () => {
        const f = await page.evaluate(() => Module._mb_frames());
        await page.waitForFunction((n) => Module._mb_frames() >= n + 2, f, { timeout: 120000 });
    };
    const stage = { screenshot: (o = {}) => page.screenshot({ ...o, clip: box, timeout: 180000 }) };
    const f0 = await page.evaluate(() => [Module._mb_frames(), Module._mb_tick(), performance.now()]);
    await page.waitForTimeout(5000);
    const f1 = await page.evaluate(() => [Module._mb_frames(), Module._mb_tick(), performance.now()]);
    const secs = (f1[2] - f0[2]) / 1000;
    const fps = (f1[0] - f0[0]) / secs;
    const tps = (f1[1] - f0[1]) / secs;
    console.log(`frames per second: ${fps.toFixed(1)}, ticks per second: ${tps.toFixed(1)} (tick ${f1[1]})`);
    fs.writeFileSync(path.join(outDir, 'fps.txt'), `${fps.toFixed(1)} fps, ${tps.toFixed(1)} ticks/s\n`);
    check(f1[0] - f0[0] >= 2, `frames are drawn (${f1[0] - f0[0]} in ${secs.toFixed(1)} s)`);
    check(f1[1] > f0[1], 'the clock ticks');
    const shot = await stage.screenshot({ path: path.join(outDir, 'chamber.png') });
    await page.screenshot({ path: path.join(outDir, 'page.png'), fullPage: true, timeout: 180000 });
    const st = await imageStats(page, shot);
    console.log(`chamber screenshot: ${st.colours} colours, mean ${st.mean.toFixed(1)}`);
    check(st.colours > 200 && st.mean > 8, 'the chamber is drawn (not a blank canvas)');

    // 3. Controls.
    const tap = async (sel) => (phone ? page.locator(sel).tap() : page.locator(sel).click());
    await tap('#pause');
    await settle();
    const t0 = await page.evaluate(() => Module._mb_tick());
    await page.waitForTimeout(1200);
    const t1 = await page.evaluate(() => Module._mb_tick());
    check(t0 === t1, `Pause stops the clock (tick ${t0} -> ${t1})`);
    check((await page.locator('#pause').textContent()).trim() === 'Resume', 'the Pause button now reads Resume');

    const cx = box.x + box.width / 2, cy = box.y + box.height / 2;
    const before = await stage.screenshot();
    if (phone) {
        const cdp = await context.newCDPSession(page);
        await touchDrag(cdp, [{ from: [cx, cy], to: [cx + box.width * 0.2, cy + box.height * 0.1] }]);
        await settle();
        const panned = await stage.screenshot({ path: path.join(outDir, 'touch-pan.png') });
        const dPan = await diffFraction(page, before, panned);
        check(dPan > 0.05, `a one-finger drag pans (${(dPan * 100).toFixed(1)} % of pixels changed)`);
        await touchDrag(cdp, [
            { from: [cx - 30, cy], to: [cx - 110, cy] },
            { from: [cx + 30, cy], to: [cx + 110, cy] },
        ]);
        await settle();
        const pinched = await stage.screenshot({ path: path.join(outDir, 'touch-pinch.png') });
        const dPinch = await diffFraction(page, panned, pinched);
        check(dPinch > 0.05, `a two-finger pinch zooms (${(dPinch * 100).toFixed(1)} % of pixels changed)`);
    } else {
        await page.mouse.move(cx, cy);
        await page.mouse.down();
        await page.mouse.move(cx + 200, cy + 80, { steps: 10 });
        await page.mouse.up();
        await settle();
        const panned = await stage.screenshot({ path: path.join(outDir, 'mouse-pan.png') });
        const dPan = await diffFraction(page, before, panned);
        check(dPan > 0.05, `a mouse drag pans (${(dPan * 100).toFixed(1)} % of pixels changed)`);
        await page.mouse.wheel(0, -400);
        await settle();
        const zoomed = await stage.screenshot({ path: path.join(outDir, 'mouse-zoom.png') });
        const dZoom = await diffFraction(page, panned, zoomed);
        check(dZoom > 0.05, `the wheel zooms (${(dZoom * 100).toFixed(1)} % of pixels changed)`);
        await page.locator('#canvas').focus();
        await page.keyboard.press('4');
        await settle();
        const oxygen = await stage.screenshot({ path: path.join(outDir, 'key-oxygen.png') });
        check(await diffFraction(page, zoomed, oxygen) > 0.05, 'key 4 shows the oxygen field');
        check(await page.locator('[data-view="3"]').getAttribute('aria-pressed') === 'true', 'the Oxygen button follows the key');
    }
    await tap('[data-view="1"]');
    await tap('#pause');
    await page.waitForTimeout(1500);
    await stage.screenshot({ path: path.join(outDir, 'nutrient.png') });

    check(errors.length === 0, `no page errors${errors.length ? ': ' + errors.join(' | ') : ''}`);
    await browser.close();
    if (failures.length) {
        console.error(`${failures.length} check(s) failed`);
        process.exit(1);
    }
    console.log('smoke test passed');
})().catch((e) => { console.error(e); process.exit(1); });
