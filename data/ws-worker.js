/* SharedWorker: single WebSocket + single IndexedDB writer
   Protocol (messages from pages):
   { type: 'init', host: string, subscribe: boolean }
   { type: 'sendText', text: string }
   Pages receive:
   { type: 'ready', dbReady: boolean, ws: 'open'|'closed' }
   { type: 'ws', state: 'open'|'closed' }
   { type: 'snapshot', data: object }
*/

const DB_NAME = 'Snapshots';
const DB_VERSION = 1;
const DB_STORE_NAME = 'snapshots';

let db = null;
let dbReady = false;
let pendingWrites = [];

let host = null;
let ws = null;
let wsState = 'closed';
let reconnectDelays = [1000, 2000, 5000, 5000, 5000];
let reconnectIdx = 0;

let latestSnapshot = null;
let flushTimer = null; // coalesce to <= 10Hz

const ports = new Set(); // { port, subscribe }

// Compaction state
let compactRunning = false;
let compactStop = false;
let compactStatus = {
  running: false,
  phase: 'idle',
  progressKey: null,
  startedAt: 0,
  finishedAt: 0,
  stats: { written: 0, deleted: 0 }
};
let compactCfg = {
  minutelyAfterSecs: 3600,      // compact to 1 min older than 1 hour
  tenMinAfterSecs: 2*24*3600,   // compact to 10 min older than 2 days
  minuteHalfWindow: 30,         // +/- 30s window
  tenHalfWindow: 300            // +/- 300s window
};

// Chart performance aggregation
let chartPerfAvg = 0;
let perfBroadcastTimer = null;

function openDb() {
  try {
    const req = indexedDB.open(DB_NAME, DB_VERSION);
    req.onupgradeneeded = (evt) => {
      const dbx = req.result;
      if (!dbx.objectStoreNames.contains(DB_STORE_NAME)) {
        dbx.createObjectStore(DB_STORE_NAME, { keyPath: 'time' });
      }
    };
    req.onsuccess = () => {
      db = req.result;
      dbReady = true;
      // flush queued writes
      if (pendingWrites.length) {
        try {
          const tx = db.transaction(DB_STORE_NAME, 'readwrite');
          const store = tx.objectStore(DB_STORE_NAME);
          while (pendingWrites.length) {
            const obj = pendingWrites.shift();
            try { store.put(obj); } catch (_) {}
          }
        } catch (_) {}
      }
      // notify all connected pages that DB is ready
      try { broadcastAll({ type: 'dbReady' }); } catch (_) {}
    };
    req.onerror = () => {
      // stay in degraded mode; pages can still get live snapshots
    };
  } catch (_) {}
}

function connectWs() {
  if (!host) return;
  try {
    ws = new WebSocket(`ws://${host}/ws`);
    ws.onopen = () => {
      wsState = 'open';
      reconnectIdx = 0;
      broadcastAll({ type: 'ws', state: 'open' });
    };
    ws.onclose = () => {
      wsState = 'closed';
      broadcastAll({ type: 'ws', state: 'closed' });
      const delay = reconnectDelays[Math.min(reconnectIdx++, reconnectDelays.length - 1)];
      setTimeout(connectWs, delay);
    };
    ws.onmessage = (evt) => {
      let obj = null;
      try { obj = JSON.parse(evt.data); } catch (_) { return; }
      if (obj) writeSnapshot(obj);
      latestSnapshot = obj;
      if (!flushTimer) {
        flushTimer = setTimeout(() => {
          flushTimer = null;
          if (latestSnapshot) broadcastSubscribed({ type: 'snapshot', data: latestSnapshot });
        }, 100);
      }
    };
  } catch (_) {}
}

function writeSnapshot(obj) {
  if (!dbReady || !db) { pendingWrites.push(obj); return; }
  try {
    const tx = db.transaction(DB_STORE_NAME, 'readwrite');
    tx.objectStore(DB_STORE_NAME).put(obj);
  } catch (_) { /* ignore */ }
}

function broadcastSubscribed(msg) {
  for (const rec of ports) {
    if (rec.subscribe) {
      try { rec.port.postMessage(msg); } catch (_) {}
    }
  }
}
function broadcastAll(msg) {
  for (const rec of ports) {
    try { rec.port.postMessage(msg); } catch (_) {}
  }
}

openDb();

self.onconnect = (e) => {
  const port = e.ports[0];
  const rec = { port, subscribe: false };
  ports.add(rec);
  port.start();
  // Notify current client count
  broadcastClients();
  port.onmessage = (ev) => {
    const msg = ev.data || {};
    if (msg.type === 'init') {
      if (!host && msg.host) { host = msg.host; connectWs(); }
      rec.subscribe = !!msg.subscribe;
      try { port.postMessage({ type: 'ready', dbReady, ws: wsState }); } catch (_) {}
    } else if (msg.type === 'compactStart') {
      if (msg.cfg) applyCompactionCfg(msg.cfg);
      startCompaction();
    } else if (msg.type === 'compactStop') {
      compactStop = true;
    } else if (msg.type === 'compactGet') {
      try { port.postMessage({ type: 'compactStatus', status: compactStatus, cfg: compactCfg }); } catch (_) {}
    } else if (msg.type === 'perfChart' && typeof msg.drawMs === 'number') {
      // Exponential moving average
      const v = msg.drawMs;
      chartPerfAvg = chartPerfAvg ? (chartPerfAvg*0.9 + v*0.1) : v;
      if (!perfBroadcastTimer) {
        perfBroadcastTimer = setTimeout(() => {
          perfBroadcastTimer = null;
          try { broadcastAll({ type: 'perfChart', avg: Math.round(chartPerfAvg), last: Math.round(v) }); } catch (_) {}
        }, 700);
      }
    } else if (msg.type === 'bye') {
      // Page is closing; remove and broadcast
      try { ports.delete(rec); } catch (_) {}
      broadcastClients();
    } else if (msg.type === 'sendText') {
      if (ws && ws.readyState === 1 && typeof msg.text === 'string') {
        try { ws.send(msg.text); } catch (_) {}
      }
    }
  };
  port.onmessageerror = () => {};
};

function broadcastClients(){
  try { broadcastAll({ type: 'clients', count: ports.size }); } catch (_) {}
}

function applyCompactionCfg(cfg){
  if (typeof cfg.minutelyAfterSecs === 'number') compactCfg.minutelyAfterSecs = cfg.minutelyAfterSecs;
  if (typeof cfg.tenMinAfterSecs === 'number') compactCfg.tenMinAfterSecs = cfg.tenMinAfterSecs;
  if (typeof cfg.minuteHalfWindow === 'number') compactCfg.minuteHalfWindow = cfg.minuteHalfWindow;
  if (typeof cfg.tenHalfWindow === 'number') compactCfg.tenHalfWindow = cfg.tenHalfWindow;
}

function startCompaction(){
  if (compactRunning) return;
  compactRunning = true;
  compactStop = false;
  compactStatus = { running: true, phase: 'start', progressKey: null, startedAt: Date.now(), finishedAt: 0, stats: { written: 0, deleted: 0 } };
  broadcastAll({ type: 'compactStatus', status: compactStatus, cfg: compactCfg });
  runCompaction().finally(() => {
    compactRunning = false;
    compactStatus.running = false;
    compactStatus.finishedAt = Date.now();
    broadcastAll({ type: 'compactStatus', status: compactStatus, cfg: compactCfg });
  });
}

async function runCompaction(){
  if (!dbReady || !db) return;
  const nowSec = Math.floor(Date.now()/1000);
  // Find oldest key
  const oldest = await findOldestKey();
  if (typeof oldest !== 'number') return;
  let key = 600 * Math.floor(oldest / 600);

  // Phase 1: 10-minute compaction for data older than tenMinAfterSecs
  compactStatus.phase = '10min';
  for (; key < nowSec - compactCfg.tenMinAfterSecs; key += 600){
    if (compactStop) return;
    compactStatus.progressKey = key;
    if (await compactBucket(key, compactCfg.tenHalfWindow)) {
      broadcastProgressThrottled();
    }
  }

  // Phase 2: 1-minute compaction for data older than minutelyAfterSecs
  compactStatus.phase = '1min';
  for (; key < nowSec - compactCfg.minutelyAfterSecs; key += 60){
    if (compactStop) return;
    compactStatus.progressKey = key;
    if (await compactBucket(key, compactCfg.minuteHalfWindow)) {
      broadcastProgressThrottled();
    }
  }
}

let lastProgressSent = 0;
function broadcastProgressThrottled(){
  const t = Date.now();
  if (t - lastProgressSent > 500){
    lastProgressSent = t;
    broadcastAll({ type: 'compactStatus', status: compactStatus, cfg: compactCfg });
  }
}

function idbRange(low, high){
  return new Promise((resolve) => {
    const out = [];
    try {
      const tx = db.transaction(DB_STORE_NAME, 'readonly');
      const store = tx.objectStore(DB_STORE_NAME);
      store.openCursor(IDBKeyRange.bound(low, high)).onsuccess = (e) => {
        const c = e.target.result; if (c){ out.push(c.value); c.continue(); } else resolve(out); };
    } catch (_) { resolve(out); }
  });
}

function idbDeleteRange(low, high){
  return new Promise((resolve) => {
    try {
      const tx = db.transaction(DB_STORE_NAME, 'readwrite');
      tx.objectStore(DB_STORE_NAME).delete(IDBKeyRange.bound(low, high));
      tx.oncomplete = () => resolve();
      tx.onerror = () => resolve();
    } catch (_) { resolve(); }
  });
}

function idbPut(obj){
  return new Promise((resolve) => {
    try {
      const tx = db.transaction(DB_STORE_NAME, 'readwrite');
      tx.objectStore(DB_STORE_NAME).put(obj);
      tx.oncomplete = () => resolve(true);
      tx.onerror = () => resolve(false);
    } catch (_) { resolve(false); }
  });
}

async function findOldestKey(){
  return new Promise((resolve) => {
    try {
      const tx = db.transaction(DB_STORE_NAME, 'readonly');
      const store = tx.objectStore(DB_STORE_NAME);
      store.openKeyCursor(null, 'next').onsuccess = (e) => {
        const cur = e.target.result; resolve(cur ? cur.key : undefined);
      };
    } catch (_) { resolve(undefined); }
  });
}

async function compactBucket(centerSec, halfWindowSec){
  const low = centerSec - halfWindowSec + 1; // exclude center to avoid deleting the new one accidentally
  const high = centerSec + halfWindowSec;
  const buf = await idbRange(centerSec - halfWindowSec, centerSec + halfWindowSec);
  if (!buf || buf.length <= 1) return false;
  const mid = Math.floor(buf.length/2);
  const rec = averageRecords(buf, mid);
  rec.time = centerSec;
  const ok = await idbPut(rec);
  if (ok){
    await idbDeleteRange(centerSec - halfWindowSec, centerSec - 1);
    await idbDeleteRange(centerSec + 1, centerSec + halfWindowSec);
    compactStatus.stats.written += 1;
    // Approx deleted count not exact
  }
  return ok;
}

function meanOf(arr, sel){
  let s=0, n=0; for (const x of arr){ const v = sel(x); if (typeof v === 'number' && isFinite(v)) { s+=v; n++; } }
  return n? s/n : undefined;
}
function averageRecords(buf, mid){
  const m = buf[mid] || {};
  return {
    target: meanOf(buf, d=>d.target),
    fnt: meanOf(buf, d=>d.fnt),
    bck: meanOf(buf, d=>d.bck),
    top: meanOf(buf, d=>d.top),
    bot: meanOf(buf, d=>d.bot),
    chip: meanOf(buf, d=>d.chip),
    rem: meanOf(buf, d=>d.rem),
    voltage: meanOf(buf, d=>d.voltage),
    current: meanOf(buf, d=>d.current),
    power: meanOf(buf, d=>d.power),
    energy: m.energy,
    pf: m.pf,
    one_pwr: m.one_pwr,
    two_pwr: m.two_pwr,
    safe: m.safe,
    blue: m.blue
  };
}
