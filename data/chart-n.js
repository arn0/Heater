// Lightweight canvas chart (no external libraries)
const gateway = `ws://${window.location.hostname}/ws`;

const DEFAULT_HOURS = 6;
const MIN_WINDOW_MS = 10 * 60 * 1000;         // 10 minutes
const MAX_WINDOW_MS = 30 * 24 * 60 * 60 * 1000;// 30 days

let db;
let dbReady = false;
const pendingSnapshots = [];
let websocket;
let swPort = null;
let canvas, ctx;
let dpr = Math.max(1, Math.min(2, window.devicePixelRatio || 1));
let plot = []; // {time(ms), rem, target, out, power, one_pwr, two_pwr}
let showRoom = true, showTarget = true, showOutside = true, showStages = true;
let followLive = true;
let lastUpdateTs = Date.now();

// Visible X window (ms)
let viewStartMs = Date.now() - DEFAULT_HOURS * 3600 * 1000;
let viewEndMs = Date.now();

// UI refs
const elShowRoom = document.getElementById('showRoom');
const elShowTarget = document.getElementById('showTarget');
const elShowOutside = document.getElementById('showOutside');
const elShowStages = document.getElementById('showStages');
const elWindow = document.getElementById('timeWindow');

window.addEventListener('load', onLoad);

function onLoad() {
  canvas = document.getElementById('chartCanvas');
  ctx = canvas.getContext('2d');
  setupResize();
  bindUI();
  openDb();
  if (!initSharedWorker()) { initWebSocket(); }
  refreshData();
}

function bindUI() {
  showRoom = elShowRoom.checked;
  showTarget = elShowTarget.checked;
  showOutside = elShowOutside.checked;
  showStages = elShowStages.checked;
  elShowRoom.addEventListener('change', () => { showRoom = elShowRoom.checked; scheduleDraw(); });
  elShowTarget.addEventListener('change', () => { showTarget = elShowTarget.checked; scheduleDraw(); });
  elShowOutside.addEventListener('change', () => { showOutside = elShowOutside.checked; scheduleDraw(); });
  elShowStages.addEventListener('change', () => { showStages = elShowStages.checked; scheduleDraw(); });
  elWindow.addEventListener('change', () => {
    const hours = parseInt(elWindow.value, 10) || DEFAULT_HOURS;
    const now = Date.now();
    viewEndMs = now;
    viewStartMs = now - hours * 3600 * 1000;
    followLive = true;
    refreshData();
  });

  // Zoom & pan
  let dragging = false;
  let dragStartX = 0;
  let dragStartStartMs = 0;
  canvas.addEventListener('mousedown', (e) => {
    dragging = true;
    dragStartX = e.offsetX * dpr;
    dragStartStartMs = viewStartMs;
    followLive = false;
  });
  window.addEventListener('mouseup', () => dragging = false);
  window.addEventListener('mousemove', (e) => {
    if (!dragging) return;
    const rect = canvas.getBoundingClientRect();
    const x = (e.clientX - rect.left) * dpr;
    const dx = x - dragStartX;
    const w = canvas.width;
    const span = viewEndMs - viewStartMs;
    const deltaMs = -dx / w * span;
    viewStartMs = dragStartStartMs + deltaMs;
    viewEndMs = viewStartMs + span;
    scheduleDraw();
  });
  canvas.addEventListener('wheel', (e) => {
    e.preventDefault();
    followLive = false;
    const rect = canvas.getBoundingClientRect();
    const x = (e.clientX - rect.left) * dpr;
    const w = canvas.width;
    const tAtCursor = xToTime(x, viewStartMs, viewEndMs, w);
    const factor = e.deltaY < 0 ? 0.9 : 1.1;
    const curSpan = viewEndMs - viewStartMs;
    let newSpan = clamp(curSpan * factor, MIN_WINDOW_MS, MAX_WINDOW_MS);
    const leftFrac = (tAtCursor - viewStartMs) / curSpan;
    viewStartMs = tAtCursor - leftFrac * newSpan;
    viewEndMs = viewStartMs + newSpan;
    scheduleDraw();
  }, { passive: false });
  canvas.addEventListener('dblclick', () => {
    const hours = parseInt(elWindow.value, 10) || DEFAULT_HOURS;
    const now = Date.now();
    viewEndMs = now;
    viewStartMs = now - hours * 3600 * 1000;
    followLive = true;
    refreshData();
  });
}

function setupResize() {
  const resize = () => {
    const cssW = Math.floor(canvas.parentElement.clientWidth);
    const cssH = Math.floor(canvas.parentElement.clientHeight);
    canvas.width = Math.max(320, cssW * dpr);
    canvas.height = Math.max(200, cssH * dpr);
    scheduleDraw();
  };
  window.addEventListener('resize', resize);
  resize();
}

// IndexedDB (same schema as index.js, condensed)
const DB_NAME = 'Snapshots';
const DB_VERSION = 1;
const DB_STORE_NAME = 'snapshots';

function openDb() {
  const req = indexedDB.open(DB_NAME, DB_VERSION);
  req.onsuccess = function () { db = this.result; dbReady = true; if (pendingSnapshots.length) { try { const store = getObjectStore(DB_STORE_NAME, 'readwrite'); for (const o of pendingSnapshots.splice(0)) { try { store.put(o); } catch (_) {} } } catch (_) {} scheduleDraw(); } };
  req.onupgradeneeded = function (evt) { evt.currentTarget.result.createObjectStore(DB_STORE_NAME, { keyPath: 'time' }); };
}
function getObjectStore(name, mode) { return db.transaction(name, mode).objectStore(name); }
function addSnapshot(obj) {
  if (!dbReady || !db) { pendingSnapshots.push(obj); return; }
  try { getObjectStore(DB_STORE_NAME, 'readwrite').put(obj); } catch (e) { /* ignore */ }
}

function initWebSocket() {
  try {
    websocket = new WebSocket(gateway);
    websocket.onopen = () => { /* online */ };
    websocket.onclose = () => setTimeout(initWebSocket, 2000);
    websocket.onmessage = (event) => {
      const update = JSON.parse(event.data);
      if (typeof update.time === 'number') lastUpdateTs = update.time * 1000;
      addSnapshot(update);
      if (followLive) {
        const span = viewEndMs - viewStartMs;
        viewEndMs = Math.max(viewEndMs, Date.now());
        viewStartMs = viewEndMs - span;
      }
      refreshData();
    };
  } catch (_) { /* ignore */ }
}

function initSharedWorker(){
  try {
    if ('SharedWorker' in window) {
      const sw = new SharedWorker('ws-worker.js');
      swPort = sw.port;
      swPort.onmessage = onWorkerMessage;
      swPort.start();
      // No live snapshots needed for chart-n; just ensure the worker is up
      swPort.postMessage({ type: 'init', host: window.location.hostname, subscribe: false });
      return true;
    }
  } catch (_) {}
  return false;
}

function onWorkerMessage(ev){
  const msg = ev.data || {};
  if (msg.type === 'dbReady' || (msg.type === 'ready' && msg.dbReady)) {
    refreshData();
  }
  if (msg.type === 'compactStatus' && msg.status && msg.status.running === false) {
    refreshData();
  }
}

function refreshData() {
  if (!db) { scheduleDraw(); return; }
  const low = Math.floor(viewStartMs / 1000);
  const high = Math.floor(viewEndMs / 1000);
  const range = IDBKeyRange.bound(low, high, false, true);
  const store = getObjectStore(DB_STORE_NAME, 'readonly');
  const req = store.openCursor(range);
  const out = [];
  req.onsuccess = (e) => {
    const cursor = e.target.result;
    if (cursor) {
      const r = cursor.value;
      out.push({
        time: r.time * 1000,
        rem: r.rem,
        target: r.target,
        out: r.out,
        power: r.power,
        one_pwr: r.one_pwr,
        two_pwr: r.two_pwr
      });
      cursor.continue();
    } else {
      out.sort((a, b) => a.time - b.time);
      plot = dedupe(out);
      scheduleDraw();
    }
  };
}

function dedupe(arr) {
  let last = -1; const res = [];
  for (const p of arr) {
    if (p.time !== last) { res.push(p); last = p.time; }
  }
  return res;
}

// Tekenlogica
let drawScheduled = false;
function scheduleDraw() { if (!drawScheduled) { drawScheduled = true; requestAnimationFrame(draw); } }

function draw() {
  const t0 = performance.now();
  drawScheduled = false;
  const w = canvas.width, h = canvas.height;
  ctx.clearRect(0, 0, w, h);
  // Padding
  const padL = 44 * dpr, padR = 20 * dpr, padT = 12 * dpr, padB = 48 * dpr;
  const chartW = Math.max(10, w - padL - padR);
  const chartH = Math.max(10, h - padT - padB);

  // Filter zichtbare punten
  const visible = plotInRange(plot, viewStartMs, viewEndMs);
  if (visible.length < 2) return drawAxesOnly();

  // Temp as | rem + target |
  let yMin = +Infinity, yMax = -Infinity;
  for (const p of visible) {
    if (typeof p.rem === 'number') { yMin = Math.min(yMin, p.rem); yMax = Math.max(yMax, p.rem); }
    if (typeof p.target === 'number') { yMin = Math.min(yMin, p.target); yMax = Math.max(yMax, p.target); }
    if (typeof p.out === 'number') { yMin = Math.min(yMin, p.out); yMax = Math.max(yMax, p.out); }
  }
  if (!isFinite(yMin) || !isFinite(yMax) || yMin === yMax) { yMin = 18; yMax = 22; }
  const yPad = 0.5;
  yMin -= yPad; yMax += yPad;

  // assen + grid
  drawGridAndAxes(padL, padT, chartW, chartH, yMin, yMax);

  // stadia achtergrond
  if (showStages) drawStages(visible, padL, padT, chartW, chartH);

  // lijnen
  const maxPts = Math.max(200, Math.floor(chartW / 2));
  if (showRoom) drawLine(decimate(visible, maxPts, v => v.rem), padL, padT, chartW, chartH, yMin, yMax, getCss('--room'));
  if (showTarget) drawLine(decimate(visible, maxPts, v => v.target), padL, padT, chartW, chartH, yMin, yMax, getCss('--target'));
  if (showOutside) drawLine(decimate(visible, maxPts, v => v.out), padL, padT, chartW, chartH, yMin, yMax, getCss('--outside'));

  function drawAxesOnly() {
    drawGridAndAxes(padL, padT, chartW, chartH, 18, 22);
  }

  // Report draw performance (throttled)
  if (swPort) {
    const dt = performance.now() - t0;
    if (!draw._lastPerf || (performance.now() - draw._lastPerf) > 500) {
      draw._lastPerf = performance.now();
      try { swPort.postMessage({ type: 'perfChart', drawMs: dt }); } catch (_) {}
    }
  }
}

function drawGridAndAxes(padL, padT, chartW, chartH, yMin, yMax) {
  const w = canvas.width, h = canvas.height;
  // Grid
  ctx.save();
  ctx.strokeStyle = getCss('--grid');
  ctx.lineWidth = 1 * dpr;
  ctx.translate(padL, padT);
  const vTicks = 5;
  for (let i = 0; i <= vTicks; i++) {
    const y = Math.round((i / vTicks) * chartH) + 0.5;
    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(chartW, y); ctx.stroke();
  }
  // Ticks X (tijd) – 6 lijnen
  const xTicks = 6;
  for (let i = 0; i <= xTicks; i++) {
    const x = Math.round((i / xTicks) * chartW) + 0.5;
    ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, chartH); ctx.stroke();
  }
  ctx.restore();

  // Y labels links
  ctx.fillStyle = getCss('--muted');
  ctx.font = `${12 * dpr}px system-ui`;
  ctx.textAlign = 'right';
  ctx.textBaseline = 'middle';
  for (let i = 0; i <= 5; i++) {
    const v = yMin + (i / 5) * (yMax - yMin);
    const y = padT + (1 - i / 5) * chartH;
    ctx.fillText(v.toFixed(1) + '°C', padL - 8 * dpr, Math.round(y));
  }

  // X labels (tijd) onder
  const xLabelTicks = 6;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'top';
  for (let i = 0; i <= xLabelTicks; i++) {
    const t = viewStartMs + (i / xLabelTicks) * (viewEndMs - viewStartMs);
    const x = padL + (i / xLabelTicks) * chartW;
    ctx.fillText(formatTime(new Date(t)), Math.round(x), padT + chartH + 6 * dpr);
  }
}

function drawStages(points, padL, padT, chartW, chartH) {
  if (points.length < 2) return;
  const span = Math.max(1, viewEndMs - viewStartMs);
  const toX = (t) => padL + (t - viewStartMs) / span * chartW;

  // 4 states: 0 none, 1 stage1, 2 stage2, 3 both
  const runs = [];
  const stateOf = (p) => (p.one_pwr && p.two_pwr) ? 3 : (p.two_pwr ? 2 : (p.one_pwr ? 1 : 0));
  let cur = stateOf(points[0]);
  let start = points[0].time;
  for (let i = 1; i < points.length; i++) {
    const s = stateOf(points[i]);
    if (s !== cur) { runs.push({ s: cur, a: start, b: points[i].time }); cur = s; start = points[i].time; }
  }
  runs.push({ s: cur, a: start, b: points[points.length - 1].time });
  // merge adj equals
  for (let i = 1; i < runs.length; ) {
    if (runs[i].s === runs[i-1].s) { runs[i-1].b = runs[i].b; runs.splice(i,1); } else i++;
  }
  // draw one color per x with no overlap between runs
  ctx.save();
  const bounds = runs.map(r => toX(r.a));
  bounds.push(toX(runs[runs.length - 1].b));
  const px = new Array(bounds.length);
  for (let i = 0; i < bounds.length; i++) {
    const v = Math.round(bounds[i]);
    px[i] = (i === 0) ? v : Math.max(v, px[i-1]);
  }
  for (let i = 0; i < runs.length; i++) {
    const r = runs[i];
    if (r.s === 0) continue;
    const x1 = px[i];
    const x2 = Math.max(px[i+1], x1 + 1);
    const width = x2 - x1;
    ctx.fillStyle = (r.s === 3) ? getCss('--stageBoth') : (r.s === 2 ? getCss('--stage2') : getCss('--stage1'));
    ctx.fillRect(x1, padT, width, chartH);
  }
  ctx.restore();
}

function drawLine(series, padL, padT, chartW, chartH, yMin, yMax, color) {
  if (series.length < 2) return;
  const span = viewEndMs - viewStartMs;
  ctx.save();
  ctx.strokeStyle = color;
  ctx.lineWidth = Math.max(1, 0.9 * dpr);
  ctx.beginPath();
  let started = false;
  for (const p of series) {
    if (!isFinite(p.y) || !isFinite(p.x)) continue;
    const x = padL + (p.x - viewStartMs) / span * chartW;
    const y = padT + (1 - (p.y - yMin) / (yMax - yMin)) * chartH;
    if (!started) { ctx.moveTo(x, y); started = true; } else { ctx.lineTo(x, y); }
  }
  ctx.stroke();
  ctx.restore();
}

function decimate(points, maxPts, accessor) {
  const out = [];
  const n = points.length;
  if (n <= maxPts) {
    for (const p of points) out.push({ x: p.time, y: accessor(p) });
    return out;
  }
  const step = Math.ceil(n / maxPts);
  for (let i = 0; i < n; i += step) {
    const p = points[i];
    out.push({ x: p.time, y: accessor(p) });
  }
  return out;
}

function plotInRange(points, start, end) {
  // eenvoudige filter (data is al gesorteerd)
  const res = [];
  for (const p of points) if (p.time >= start && p.time <= end) res.push(p);
  return res;
}

function xToTime(x, start, end, width) {
  const frac = clamp(x / Math.max(1, width), 0, 1);
  return start + frac * (end - start);
}

function clamp(v, a, b) { return Math.min(b, Math.max(a, v)); }
function getCss(name) { return getComputedStyle(document.documentElement).getPropertyValue(name).trim(); }
function formatTime(d) { return d.toLocaleTimeString('nl-NL', { hour: '2-digit', minute: '2-digit' }); }

function notifyWorkerBye(){ if (swPort) { try { swPort.postMessage({ type:'bye' }); } catch(_) {} } }
window.addEventListener('beforeunload', notifyWorkerBye);
window.addEventListener('pagehide', notifyWorkerBye);
window.addEventListener('unload', notifyWorkerBye);
