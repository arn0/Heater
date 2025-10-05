// Lightweight canvas chart (no external libraries)
const gateway = `ws://${window.location.hostname}/ws`;

const DEFAULT_HOURS = 6;
const MIN_WINDOW_MS = 10 * 60 * 1000;         // 10 minutes
const MAX_WINDOW_MS = 30 * 24 * 60 * 60 * 1000;// 30 days

let db;
let websocket;
let canvas, ctx;
let dpr = Math.max(1, Math.min(2, window.devicePixelRatio || 1));
let plot = []; // {time(ms), rem, target, power, one_pwr, two_pwr}
let showRoom = true, showTarget = true, showPower = true, showStages = true;
let followLive = true;
let lastUpdateTs = Date.now();

// Visible X window (ms)
let viewStartMs = Date.now() - DEFAULT_HOURS * 3600 * 1000;
let viewEndMs = Date.now();

// UI refs
const elShowRoom = document.getElementById('showRoom');
const elShowTarget = document.getElementById('showTarget');
const elShowPower = document.getElementById('showPower');
const elShowStages = document.getElementById('showStages');
const elWindow = document.getElementById('timeWindow');

window.addEventListener('load', onLoad);

function onLoad() {
  canvas = document.getElementById('chartCanvas');
  ctx = canvas.getContext('2d');
  setupResize();
  bindUI();
  openDb();
  initWebSocket();
  refreshData();
}

function bindUI() {
  elShowRoom.addEventListener('change', () => { showRoom = elShowRoom.checked; scheduleDraw(); });
  elShowTarget.addEventListener('change', () => { showTarget = elShowTarget.checked; scheduleDraw(); });
  elShowPower.addEventListener('change', () => { showPower = elShowPower.checked; scheduleDraw(); });
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
  req.onsuccess = function () { db = this.result; };
  req.onupgradeneeded = function (evt) { evt.currentTarget.result.createObjectStore(DB_STORE_NAME, { keyPath: 'time' }); };
}
function getObjectStore(name, mode) { return db.transaction(name, mode).objectStore(name); }
function addSnapshot(obj) {
  try { getObjectStore(DB_STORE_NAME, 'readwrite').put(obj); } catch (e) { /* no-op */ }
}

// WebSocket
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
      // omzet naar ms
      out.push({
        time: r.time * 1000,
        rem: r.rem,
        target: r.target,
        power: r.power,
        one_pwr: r.one_pwr,
        two_pwr: r.two_pwr
      });
      cursor.continue();
    } else {
      // sorteren en deduplicatie op tijd
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
  if (showRoom) drawLine(decimate(visible, maxPts, v => v.rem), padL, padT, chartW, chartH, yMin, yMax, '#d22');
  if (showTarget) drawLine(decimate(visible, maxPts, v => v.target), padL, padT, chartW, chartH, yMin, yMax, '#b0b');

  // vermogen in subpaneel onderaan
  if (showPower) drawPower(visible, padL, padT, chartW, chartH, w, h);

  function drawAxesOnly() {
    drawGridAndAxes(padL, padT, chartW, chartH, 18, 22);
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

  // Build/merge runs for a predicate, close tiny gaps (3px or <=60s)
  const runsFrom = (predicate) => {
    const list = [];
    let cur = predicate(points[0]);
    let start = points[0].time;
    for (let i = 1; i < points.length; i++) {
      const v = predicate(points[i]);
      if (v !== cur) { list.push({ v: cur, a: start, b: points[i].time }); cur = v; start = points[i].time; }
    }
    list.push({ v: cur, a: start, b: points[points.length - 1].time });
    // merge adjacent equals
    for (let i = 1; i < list.length; ) {
      if (list[i].v === list[i-1].v) { list[i-1].b = list[i].b; list.splice(i,1); } else i++;
    }
    // close short zero gaps between non-zero runs
    for (let i = 1; i < list.length - 1; ) {
      const mid = list[i];
      if (!mid.v && list[i-1].v && list[i+1].v) {
        const gapPx = Math.ceil(toX(mid.b)) - Math.floor(toX(mid.a));
        const gapSec = (mid.b - mid.a) / 1000;
        if (gapPx <= 3 || gapSec <= 60) { list[i-1].b = list[i+1].b; list.splice(i,2); continue; }
      }
      i++;
    }
    return list;
  };

  const runs1 = runsFrom(p => (p.one_pwr && !p.two_pwr) ? 1 : 0);
  const runs2 = runsFrom(p => (p.two_pwr && !p.one_pwr) ? 1 : 0);
  const runsBoth = runsFrom(p => (p.one_pwr && p.two_pwr) ? 1 : 0);

  const drawRuns = (runs, colorVar) => {
    ctx.save();
    ctx.fillStyle = getCss(colorVar);
    for (const r of runs) {
      if (!r.v) continue;
      const x1 = Math.floor(toX(r.a));
      const x2 = Math.ceil(toX(r.b));
      const width = Math.max(1, x2 - x1);
      ctx.fillRect(x1, padT, width, chartH);
    }
    ctx.restore();
  };
  drawRuns(runs1, '--stage1');
  drawRuns(runs2, '--stage2');
  drawRuns(runsBoth, '--stageBoth');
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

function drawPower(points, padL, padT, chartW, chartH, w, h) {
  const bandH = Math.max(32 * dpr, Math.min(64 * dpr, Math.floor(h * 0.18)));
  const y0 = padT + chartH + 4 * dpr;
  // achtergrond
  ctx.save();
  ctx.fillStyle = '#fafafa';
  ctx.fillRect(padL, y0, chartW, bandH);
  // schaal
  let maxP = 0;
  for (const p of points) if (typeof p.power === 'number') maxP = Math.max(maxP, p.power);
  if (maxP <= 0) { ctx.restore(); return; }
  const span = viewEndMs - viewStartMs;
  ctx.strokeStyle = getCss('--power');
  ctx.lineWidth = Math.max(1, 0.9 * dpr);
  ctx.beginPath();
  let started = false;
  const maxPts = Math.max(200, Math.floor(chartW / 2));
  const series = decimate(points, maxPts, v => v.power);
  for (const p of series) {
    const x = padL + (p.x - viewStartMs) / span * chartW;
    const y = y0 + bandH - (p.y / maxP) * bandH;
    if (!started) { ctx.moveTo(x, y); started = true; } else { ctx.lineTo(x, y); }
  }
  ctx.stroke();
  // schaal label rechts
  ctx.fillStyle = getCss('--muted');
  ctx.font = `${12 * dpr}px system-ui`;
  ctx.textAlign = 'right'; ctx.textBaseline = 'top';
  ctx.fillText(`${Math.round(maxP)} W`, padL + chartW - 4 * dpr, y0 + 2 * dpr);
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
