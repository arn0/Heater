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
const CFG_DB_NAME = 'WorkerSettings';
const CFG_STORE_NAME = 'compaction';

let db = null;
let dbReady = false;
let pendingWrites = [];
let cfgDb = null;

let host = null;
let ws = null;
let wsState = 'closed';
let reconnectDelays = [1000, 2000, 5000, 5000, 5000];
let reconnectIdx = 0;

let latestSnapshot = null;
let flushTimer = null; // coalesce to <= 10Hz

const KNMI_MIN_WINDOW = 600;
const KNMI_MAX_WINDOW = 6 * 3600;
let knmiWindowSecs = KNMI_MIN_WINDOW;
let knmiWindowLastSent = 0;
let pendingKnmiPayload = null;
let knmiWindowTimer = null;
let knmiRecalcScheduled = false;

const BACKFILL_WINDOW_SECS = 1 * 24 * 3600;
const BACKFILL_INTERVAL_MS = 1 * 60 * 1000;
const BACKFILL_RETRY_MS = 1 * 60 * 1000;
const BACKFILL_MAX_ATTEMPTS = 5;
const BACKFILL_SKIP_MS = 6 * 60 * 60 * 1000;
const BACKFILL_STEP_SECS = 600; // KNMI data cadence (10 minutes)
const BAD_OUT_VALUE = 5.7067;
const BAD_OUT_TOLERANCE = 0.005;
let knmiDryRun = false;

let backfillTimer = null;
let backfillInFlight = null;
let backfillQueue = [];
const backfillFailures = new Map(); // windowStart -> attempt count
const backfillSkipped = new Map(); // windowStart -> timestamp (ms) until retry allowed
const missingSnapshotLogged = new Set();
let currentSchedule = null;

function shouldSkipBackfillWindow(windowStart) {
  if (!Number.isFinite(windowStart)) return false;
  const expiry = backfillSkipped.get(windowStart);
  if (!expiry) return false;
  if (Date.now() >= expiry) {
    backfillSkipped.delete(windowStart);
    return false;
  }
  return true;
}

function registerBackfillFailure(windowStart, reason) {
  if (!Number.isFinite(windowStart)) return { retry: false, skipped: false };
  const attempts = (backfillFailures.get(windowStart) || 0) + 1;
  backfillFailures.set(windowStart, attempts);
  const iso = new Date(windowStart * 1000).toISOString();
  const why = reason ? String(reason) : 'unknown';
  if (attempts >= BACKFILL_MAX_ATTEMPTS) {
    backfillFailures.delete(windowStart);
    const until = Date.now() + BACKFILL_SKIP_MS;
    backfillSkipped.set(windowStart, until);
    console.log('[KNMI] skipping backfill window', iso, 'for', Math.round(BACKFILL_SKIP_MS / 60000), 'minutes (reason:', why + ')');
    return { retry: false, skipped: true };
  }
  console.log('[KNMI] backfill retry', iso, 'attempt', attempts, 'reason:', why);
  return { retry: true, skipped: false };
}

function clearBackfillState(windowStart) {
  if (!Number.isFinite(windowStart)) return;
  backfillFailures.delete(windowStart);
  backfillSkipped.delete(windowStart);
}

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
  stats: { written: 0, deleted: 0 },
  error: null
};
let compactCfg = {
  stages: [
    { olderThanSecs: 3600, targetIntervalSecs: 60 },
    { olderThanSecs: 2 * 24 * 3600, targetIntervalSecs: 600 },
    { olderThanSecs: 5 * 24 * 3600, targetIntervalSecs: 3600 }
  ]
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
          tx.oncomplete = () => scheduleKnmiWindowCheck();
        } catch (_) {}
      } else {
        scheduleKnmiWindowCheck();
      }
      scheduleBackfill(10 * 1000);
      // notify all connected pages that DB is ready
      try { broadcastAll({ type: 'dbReady' }); } catch (_) {}
    };
    req.onerror = () => {
      // stay in degraded mode; pages can still get live snapshots
    };
  } catch (_) {}
}

function openConfigDb() {
  try {
    const req = indexedDB.open(CFG_DB_NAME, 1);
    req.onupgradeneeded = () => {
      const dbx = req.result;
      if (!dbx.objectStoreNames.contains(CFG_STORE_NAME)) {
        dbx.createObjectStore(CFG_STORE_NAME, { keyPath: 'id' });
      }
    };
    req.onsuccess = () => {
      cfgDb = req.result;
      loadCompactCfgFromStore();
    };
  } catch (_) {}
}

function loadCompactCfgFromStore() {
  if (!cfgDb) return;
  try {
    const tx = cfgDb.transaction(CFG_STORE_NAME, 'readonly');
    const store = tx.objectStore(CFG_STORE_NAME);
    const getReq = store.get('cfg');
    getReq.onsuccess = () => {
      const data = getReq.result;
      if (data && data.cfg) {
        if (applyCompactionCfg(data.cfg, true)) {
          broadcastAll({ type: 'compactStatus', status: compactStatus, cfg: compactCfg });
        }
      } else {
        persistCompactCfg();
        broadcastAll({ type: 'compactStatus', status: compactStatus, cfg: compactCfg });
      }
    };
  } catch (_) {}
}

function persistCompactCfg() {
  if (!cfgDb) return;
  try {
    const tx = cfgDb.transaction(CFG_STORE_NAME, 'readwrite');
    tx.objectStore(CFG_STORE_NAME).put({ id: 'cfg', cfg: compactCfg });
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
      trySendKnmiWindow();
      trySendBackfillQueue();
    };
    ws.onclose = () => {
      wsState = 'closed';
      broadcastAll({ type: 'ws', state: 'closed' });
      if (backfillInFlight) {
        if (backfillInFlight.timeout) {
          clearTimeout(backfillInFlight.timeout);
        }
        if (backfillInFlight.payload) {
          backfillQueue.unshift(backfillInFlight.payload);
        }
        backfillInFlight = null;
      }
      scheduleBackfill(BACKFILL_RETRY_MS);
      const delay = reconnectDelays[Math.min(reconnectIdx++, reconnectDelays.length - 1)];
      setTimeout(connectWs, delay);
    };
    ws.onmessage = (evt) => {
      let obj = null;
      try { obj = JSON.parse(evt.data); } catch (_) { return; }
      if (!obj) return;
      if (obj && typeof obj === 'object' && obj.type === 'knmi_series') {
        handleKnmiSeriesResponse(obj);
        return;
      }
      if (obj && typeof obj === 'object' && obj.type === 'schedule') {
        handleScheduleMessage(obj);
        return;
      }
      writeSnapshot(obj);
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
    tx.oncomplete = () => scheduleKnmiWindowCheck();
  } catch (_) { /* ignore */ }
}

function broadcastSubscribed(msg) {
  let changed = false;
  for (const rec of Array.from(ports)) {
    if (rec.subscribe) {
      if (!postToPort(rec, msg)) changed = true;
    }
  }
  if (changed) broadcastClients();
}
function broadcastAll(msg) {
  let changed = false;
  for (const rec of Array.from(ports)) {
    if (!postToPort(rec, msg)) changed = true;
  }
  if (changed) broadcastClients();
}

openDb();
openConfigDb();

self.onconnect = (e) => {
  const port = e.ports[0];
  const rec = { port, subscribe: false };
  ports.add(rec);
  port.start();
// ---- SAFE MINI-BRIDGE: stuur logs naar alle open ports ----
if (!console.__bridged) {
  const __orig = console.log.bind(console);
  console.log = (...args) => {
    try { ports.forEach(r => r.port.postMessage({ __sw_log: args })); } catch {}
    __orig(...args);
  };
  console.__bridged = true;
}
// ------------------------------------------------------------
  console.log('SharedWorker started');
// Notify current client count
  broadcastClients();
  port.onmessage = (ev) => {
    const msg = ev.data || {};
    if (msg.type === 'init') {
      if (!host && msg.host) { host = msg.host; connectWs(); }
      rec.subscribe = !!msg.subscribe;
      try { port.postMessage({ type: 'ready', dbReady, ws: wsState }); } catch (_) {}
    } else if (msg.type === 'compactStart') {
      if (msg.cfg && !applyCompactionCfg(msg.cfg)) {
        compactStatus.running = false;
        compactStatus.phase = 'idle';
        compactStatus.error = 'Ongeldige configuratie';
        broadcastAll({ type: 'compactStatus', status: compactStatus, cfg: compactCfg });
        return;
      }
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
      try { rec.port.close && rec.port.close(); } catch (_) {}
      broadcastClients();
    } else if (msg.type === 'sendText') {
      if (ws && ws.readyState === 1 && typeof msg.text === 'string') {
        try { ws.send(msg.text); } catch (_) {}
      }
    }
  };
  port.onmessageerror = () => {};
  if (currentSchedule && currentSchedule.schedule) {
    const msg = { type: 'schedule', schedule: { ...currentSchedule.schedule } };
    if (currentSchedule.config) {
      msg.config = { ...currentSchedule.config };
    }
    try { port.postMessage(msg); } catch (_) {}
  }
};

function broadcastClients(){
  const msg = { type: 'clients', count: ports.size };
  let changed = false;
  for (const rec of Array.from(ports)) {
    if (!postToPort(rec, msg)) changed = true;
  }
  if (changed) broadcastClients();
}

function handleScheduleMessage(msg) {
  const payload = extractSchedulePayload(msg);
  if (!payload || !payload.schedule) return;
  currentSchedule = payload;
  const out = { type: 'schedule', schedule: { ...payload.schedule } };
  if (payload.config) {
    out.config = { ...payload.config };
  }
  broadcastAll(out);
}

function extractSchedulePayload(msg) {
  if (!msg || typeof msg !== 'object') return null;
  const out = {};
  if (msg.schedule && typeof msg.schedule === 'object') {
    out.schedule = { ...msg.schedule };
  } else if (msg.data && typeof msg.data === 'object') {
    out.schedule = { ...msg.data };
  } else {
    const copy = { ...msg };
    if (copy.type) delete copy.type;
    out.schedule = copy;
  }
  if (msg.config && typeof msg.config === 'object') {
    out.config = { ...msg.config };
  } else if (out.schedule && typeof out.schedule.config === 'object') {
    out.config = { ...out.schedule.config };
    delete out.schedule.config;
  }
  return out;
}

function postToPort(rec, msg) {
  try {
    rec.port.postMessage(msg);
    return true;
  } catch (_) {
    try { rec.port.close && rec.port.close(); } catch (_) {}
    ports.delete(rec);
    return false;
  }
}

function scheduleKnmiWindowCheck(delayMs = 0) {
  if (knmiRecalcScheduled) return;
  knmiRecalcScheduled = true;
  setTimeout(() => {
    knmiRecalcScheduled = false;
    computeKnmiWindow();
  }, Math.max(0, delayMs));
}

function computeKnmiWindow() {
  if (!dbReady || !db) return;
  const MAX_SCAN = 720;
  let latestTime = null;
  let lastValidTime = null;
  let scanned = 0;
  let finished = false;
  try {
    const tx = db.transaction(DB_STORE_NAME, 'readonly');
    const store = tx.objectStore(DB_STORE_NAME);
    const req = store.openCursor(null, 'prev');
    req.onsuccess = (ev) => {
      if (finished) return;
      const cursor = ev.target.result;
      if (!cursor) {
        finalize();
        return;
      }
      const record = cursor.value;
      if (record && typeof record.time === 'number') {
        if (latestTime === null) latestTime = record.time;
        if (!needsOutBackfill(record)) {
          lastValidTime = record.time;
          finalize();
          return;
        }
      }
      if (++scanned >= MAX_SCAN) {
        finalize();
        return;
      }
      cursor.continue();
    };
    req.onerror = () => finalize();
    tx.oncomplete = () => finalize();
  } catch (_) {
    finalize();
  }

  function finalize() {
    if (finished) return;
    finished = true;
    let desired = KNMI_MIN_WINDOW;
    if (latestTime !== null) {
      if (lastValidTime !== null) {
        const gapSecs = Math.max(0, latestTime - lastValidTime);
        if (gapSecs > 0) {
          desired = Math.min(KNMI_MAX_WINDOW, Math.max(KNMI_MIN_WINDOW, gapSecs + KNMI_MIN_WINDOW));
        }
      } else {
        const nowSecs = Math.floor(Date.now() / 1000);
        const ageSecs = Math.max(0, nowSecs - latestTime);
        desired = Math.min(KNMI_MAX_WINDOW, Math.max(KNMI_MIN_WINDOW, ageSecs + KNMI_MIN_WINDOW));
      }
    }
    sendKnmiWindow(desired);
  }
}

function sendKnmiWindow(windowSecs) {
  if (!Number.isFinite(windowSecs)) return;
  let adjusted = Math.round(windowSecs / 60) * 60;
  if (!Number.isFinite(adjusted) || adjusted <= 0) adjusted = KNMI_MIN_WINDOW;
  adjusted = Math.max(KNMI_MIN_WINDOW, Math.min(KNMI_MAX_WINDOW, adjusted));
  if (Math.abs(adjusted - knmiWindowSecs) < 60) return;
  knmiWindowSecs = adjusted;
  pendingKnmiPayload = JSON.stringify({ type: 'knmi', window_secs: adjusted });
  trySendKnmiWindow();
}

function trySendKnmiWindow() {
  if (!pendingKnmiPayload) return;
  const now = Date.now();
  const throttleMs = 5000;
  if (now - knmiWindowLastSent < throttleMs) {
    const wait = throttleMs - (now - knmiWindowLastSent);
    if (!knmiWindowTimer) {
      knmiWindowTimer = setTimeout(() => {
        knmiWindowTimer = null;
        trySendKnmiWindow();
      }, Math.max(50, wait));
    }
    return;
  }
  if (ws && ws.readyState === 1) {
    try {
      ws.send(pendingKnmiPayload);
      knmiWindowLastSent = now;
      pendingKnmiPayload = null;
    } catch (_) {
      if (!knmiWindowTimer) {
        knmiWindowTimer = setTimeout(() => {
          knmiWindowTimer = null;
          trySendKnmiWindow();
        }, throttleMs);
      }
    }
  } else {
    if (!knmiWindowTimer) {
      knmiWindowTimer = setTimeout(() => {
        knmiWindowTimer = null;
        trySendKnmiWindow();
      }, 1000);
    }
  }
}


function dirtyReasons(record) {
  const reasons = [];
  if (!record) return reasons;
  const value = record.out;
  if (typeof value !== 'number' || !Number.isFinite(value)) reasons.push('out_missing');
  else if (Math.abs(value - BAD_OUT_VALUE) < BAD_OUT_TOLERANCE) reasons.push('out_invalid');
  if ('pf' in record) reasons.push('has_pf');
  if (record.schedule) reasons.push('has_schedule');
  if (record.config) reasons.push('has_config');
  return reasons;
}

function needsOutBackfill(record) {
  if (!record) return false;
  const value = record.out;
  if (typeof value !== 'number' || !Number.isFinite(value)) return true;
  return Math.abs(value - BAD_OUT_VALUE) < BAD_OUT_TOLERANCE;
}

function prepareBackfillUpdate(record, temp) {
  if (!record) return { modified: false, updated: null, reasons: [] };
  const reasons = dirtyReasons(record);
  if (!reasons.length) return { modified: false, updated: null, reasons };
  const updated = { ...record };
  let modified = false;
  if (reasons.includes('has_schedule') && 'schedule' in updated) {
    delete updated.schedule;
    modified = true;
  }
  if (reasons.includes('has_config') && 'config' in updated) {
    delete updated.config;
    modified = true;
  }
  if (reasons.includes('has_pf') && 'pf' in updated) {
    delete updated.pf;
    modified = true;
  }
  if (reasons.includes('out_missing') || reasons.includes('out_invalid')) {
    updated.out = temp;
    modified = true;
  }
  return { modified, updated, reasons };
}

function debugLogNearbySnapshots(store, target) {
  try {
    const delta = 1800; // +/-30 min window
    const lower = Math.max(1, Math.floor(target - delta));
    const upper = Math.max(lower, Math.floor(target + delta));
    const range = IDBKeyRange.bound(lower, upper);
    const nearby = [];
    store.openCursor(range).onsuccess = (ev) => {
      const cursor = ev.target.result;
      if (!cursor) {
        if (nearby.length) {
          console.log('[KNMI] nearby snapshots around', new Date(target * 1000).toISOString(), nearby);
        } else {
          console.log('[KNMI] no nearby snapshots around', new Date(target * 1000).toISOString());
        }
        return;
      }
      const val = cursor.value;
      if (val && typeof val.time !== 'undefined') {
        nearby.push({
          time: val.time,
          out: val.out,
          hasPf: 'pf' in val,
          hasSchedule: !!val.schedule,
          hasConfig: !!val.config
        });
        if (nearby.length >= 5) {
          console.log('[KNMI] nearby snapshots around', new Date(target * 1000).toISOString(), nearby);
          return;
        }
      }
      cursor.continue();
    };
  } catch (_) {
    // ignore
  }
}

function fetchSnapshotForTime(store, originalTime, callback) {
  const target = Number(originalTime);
  const tried = new Set();
  let settled = false;

  function finish(record, keyUsed) {
    if (settled) return;
    settled = true;
    if (!record && Number.isFinite(target) && !missingSnapshotLogged.has(target)) {
      missingSnapshotLogged.add(target);
      debugLogNearbySnapshots(store, target);
    }
    callback(record || null, keyUsed);
  }

  function tryGet(key) {
    if (key === undefined || key === null) {
      return false;
    }
    if (tried.has(key)) {
      return false;
    }
    tried.add(key);
    let req;
    try {
      req = store.get(key);
    } catch (_) {
      return false;
    }
    req.onsuccess = () => {
      const record = req.result;
      if (record !== undefined && record !== null) {
        finish(record, key);
      } else {
        nextAttempt();
      }
    };
    req.onerror = () => nextAttempt();
    return true;
  }

  function scanAll() {
    try {
      const cursorReq = store.openCursor();
      cursorReq.onsuccess = (ev) => {
        const cursor = ev.target.result;
        if (!cursor) {
          finish(null, target);
          return;
        }
        const curKey = cursor.key;
        if (Number(curKey) === target || String(curKey) === String(originalTime)) {
          finish(cursor.value || null, curKey);
          return;
        }
        cursor.continue();
      };
      cursorReq.onerror = () => finish(null, target);
    } catch (_) {
      finish(null, target);
    }
  }

  function nextAttempt() {
    if (attempts.length) {
      const key = attempts.shift();
      if (!tryGet(key)) {
        nextAttempt();
      }
      return;
    }
    scanAll();
  }

  const attempts = [];
  if (Number.isFinite(target)) {
    attempts.push(target, String(target));
  }
  if (typeof originalTime === 'string' && !attempts.includes(originalTime)) {
    attempts.push(originalTime);
    const parsed = Number(originalTime);
    if (Number.isFinite(parsed) && !attempts.includes(parsed)) attempts.push(parsed);
  }

  nextAttempt();
}

function scheduleBackfill(delayMs = BACKFILL_INTERVAL_MS) {
  if (backfillTimer) {
    clearTimeout(backfillTimer);
  }
  backfillTimer = setTimeout(runBackfill, Math.max(0, delayMs));
}

function runBackfill() {
  backfillTimer = null;
  if (!dbReady || !db) {
    scheduleBackfill(BACKFILL_INTERVAL_MS);
    return;
  }
  if (!backfillInFlight) {
    trySendBackfillQueue();
    if (backfillInFlight) {
      return;
    }
  }
  if (backfillInFlight) {
    return;
  }
  findNextMissingOut().then((start) => {
    if (!Number.isFinite(start)) {
      scheduleBackfill(BACKFILL_INTERVAL_MS);
      return;
    }
    const windowStart = BACKFILL_STEP_SECS * Math.floor(Math.max(1, start) / BACKFILL_STEP_SECS);
    const windowEnd = windowStart + BACKFILL_WINDOW_SECS;
    const pendingSame = (backfillInFlight && backfillInFlight.payload && backfillInFlight.payload.start === windowStart) ||
      backfillQueue.some((item) => item.start === windowStart);
    if (pendingSame) {
      scheduleBackfill(BACKFILL_RETRY_MS);
      return;
    }
    const payload = {
      type: 'knmi_series',
      start: windowStart,
      end: windowEnd,
      request_id: `knmi-bf-${windowStart}-${Date.now()}`
    };
    console.log('[KNMI] queue backfill window', new Date(windowStart * 1000).toISOString(), 'to', new Date(windowEnd * 1000).toISOString());
    backfillQueue.push(payload);
    trySendBackfillQueue();
    if (!backfillInFlight) {
      scheduleBackfill(BACKFILL_RETRY_MS);
    }
  }).catch(() => {
    scheduleBackfill(BACKFILL_RETRY_MS);
  });
}

function findNextMissingOut() {
  return new Promise((resolve) => {
    try {
      const tx = db.transaction(DB_STORE_NAME, 'readonly');
      const store = tx.objectStore(DB_STORE_NAME);
      const req = store.openCursor();
      req.onsuccess = (ev) => {
        const cursor = ev.target.result;
        if (!cursor) {
          resolve(null);
          return;
        }
        const record = cursor.value;
        if (record && typeof record.time === 'number') {
          const time = Number(record.time);
          if (shouldSkipBackfillWindow(time)) {
            cursor.continue();
            return;
          }
          const reasons = dirtyReasons(record);
          if (reasons.length) {
            console.log('[KNMI] next dirty snapshot', new Date(record.time * 1000).toISOString(), 'reasons', reasons);
            resolve(record.time);
            return;
          }
        }
        cursor.continue();
      };
      req.onerror = () => resolve(null);
    } catch (_) {
      resolve(null);
    }
  });
}

function trySendBackfillQueue() {
  if (backfillInFlight) return;
  if (!ws || ws.readyState !== 1) return;
  const payload = backfillQueue.shift();
  if (!payload) return;
  const text = JSON.stringify(payload);
  try {
    ws.send(text);
    backfillInFlight = {
      requestId: payload.request_id || null,
      payload,
      sentAt: Date.now(),
      timeout: setTimeout(() => {
        if (backfillInFlight && backfillInFlight.payload === payload) {
          const iso = Number.isFinite(payload.start) ? new Date(payload.start * 1000).toISOString() : payload.start;
          console.log('[KNMI] backfill response timeout for window starting', iso);
          backfillInFlight = null;
          const outcome = registerBackfillFailure(payload.start, 'timeout');
          if (outcome.retry) {
            backfillQueue.unshift(payload);
          }
          scheduleBackfill(outcome.retry ? BACKFILL_RETRY_MS : BACKFILL_INTERVAL_MS);
        }
      }, BACKFILL_RETRY_MS)
    };
  } catch (err) {
    registerBackfillFailure(payload.start, err && err.message ? err.message : 'ws_send_failed');
    backfillInFlight = null;
    backfillQueue.unshift(payload);
    scheduleBackfill(BACKFILL_RETRY_MS);
  }
}

function handleKnmiSeriesResponse(msg) {
  const requestId = msg && typeof msg.request_id === 'string' ? msg.request_id : null;
  const inflight = backfillInFlight;
  if (inflight && (!inflight.requestId || inflight.requestId === requestId)) {
    if (inflight.timeout) {
      clearTimeout(inflight.timeout);
    }
    backfillInFlight = null;
  }
  if (!msg || msg.ok === false) {
    const payload = inflight && inflight.payload;
    let delay = BACKFILL_RETRY_MS;
    if (payload && Number.isFinite(payload.start)) {
      const res = registerBackfillFailure(payload.start, msg && msg.error ? msg.error : 'knmi_error');
      delay = res.retry ? BACKFILL_RETRY_MS : BACKFILL_INTERVAL_MS;
    }
    scheduleBackfill(delay);
    return;
  }
  const samples = Array.isArray(msg.samples) ? msg.samples : [];
  const payload = inflight ? inflight.payload : null;
  if (samples.length) {
    const firstTs = Number(samples[0][0]);
    const lastTs = Number(samples[samples.length - 1][0]);
    console.log('[KNMI] samples window', samples.length, 'first', Number.isFinite(firstTs) ? new Date(firstTs * 1000).toISOString() : firstTs, 'last', Number.isFinite(lastTs) ? new Date(lastTs * 1000).toISOString() : lastTs);
  } else {
    console.log('[KNMI] samples window empty');
  }
  applyBackfillSamples(samples, payload).then((updated) => {
    if (samples.length) {
      console.log(`[KNMI] backfill applied ${updated}/${samples.length} samples`);
    }
    let delay = BACKFILL_INTERVAL_MS;
    if (payload && Number.isFinite(payload.start)) {
      if (updated > 0) {
        clearBackfillState(payload.start);
      } else {
        const reason = samples.length === 0 ? 'no_samples' : 'no_updates';
        const res = registerBackfillFailure(payload.start, reason);
        delay = res.retry ? BACKFILL_RETRY_MS : BACKFILL_INTERVAL_MS;
      }
    }
    if (updated > 0) {
      scheduleKnmiWindowCheck(0);
    }
    scheduleBackfill(delay);
  }).catch((err) => {
    if (payload && Number.isFinite(payload.start)) {
      registerBackfillFailure(payload.start, err && err.message ? err.message : 'apply_failed');
    }
    scheduleBackfill(BACKFILL_RETRY_MS);
  });
}

function applyBackfillSamples(samples, payload) {
  return new Promise((resolve) => {
    if (!dbReady || !db) {
      resolve(0);
      return;
    }
    const sampleList = [];
    if (Array.isArray(samples)) {
      for (const entry of samples) {
        if (!Array.isArray(entry) || entry.length < 2) continue;
        const time = Number(entry[0]);
        const temp = Number(entry[1]);
        if (!Number.isFinite(time) || !Number.isFinite(temp)) continue;
        sampleList.push({ time: Math.round(time), temp });
      }
    }
    sampleList.sort((a, b) => a.time - b.time);
    if (!sampleList.length) {
      resolve(0);
      return;
    }
    const windowStart = payload && Number.isFinite(payload.start)
      ? Math.max(1, BACKFILL_STEP_SECS * Math.floor(Number(payload.start) / BACKFILL_STEP_SECS))
      : null;
    const windowEnd = windowStart !== null ? windowStart + BACKFILL_WINDOW_SECS : null;

    function estimateTemp(targetTime) {
      if (!Number.isFinite(targetTime) || !sampleList.length) return null;
      const t = Math.round(targetTime);
      let lo = 0;
      let hi = sampleList.length - 1;
      while (lo <= hi) {
        const mid = (lo + hi) >> 1;
        const midTime = sampleList[mid].time;
        if (midTime === t) {
          return sampleList[mid].temp;
        }
        if (midTime < t) lo = mid + 1;
        else hi = mid - 1;
      }
      const prev = hi >= 0 ? sampleList[hi] : null;
      const next = lo < sampleList.length ? sampleList[lo] : null;
      if (prev && next) {
        if (next.time === prev.time) return prev.temp;
        const ratio = (t - prev.time) / (next.time - prev.time);
        return prev.temp + (next.temp - prev.temp) * ratio;
      }
      if (prev) return prev.temp;
      if (next) return next.temp;
      return null;
    }

    let updated = 0;
    try {
      const tx = db.transaction(DB_STORE_NAME, 'readwrite');
      const store = tx.objectStore(DB_STORE_NAME);
      const range = (Number.isFinite(windowStart) && Number.isFinite(windowEnd) && windowEnd >= windowStart)
        ? IDBKeyRange.bound(windowStart, windowEnd)
        : null;
      const cursorReq = range ? store.openCursor(range) : store.openCursor();
      cursorReq.onsuccess = (ev) => {
        const cursor = ev.target.result;
        if (!cursor) return;
        const record = cursor.value;
        if (!record || typeof record.time !== 'number') {
          cursor.continue();
          return;
        }
        const reasons = dirtyReasons(record);
        if (!reasons.length) {
          cursor.continue();
          return;
        }
        const temp = estimateTemp(record.time);
        const iso = new Date(record.time * 1000).toISOString();
        if (!Number.isFinite(temp)) {
          console.log('[KNMI] no temperature available for snapshot', iso, '– keeping dirty');
          cursor.continue();
          return;
        }
        console.log('[KNMI] updating snapshot', iso, 'dirtyReasons', reasons, 'newTemp', temp);
        if (knmiDryRun) {
          cursor.continue();
          return;
        }
        let modified = false;
        if (record.schedule) { delete record.schedule; modified = true; }
        if (record.config) { delete record.config; modified = true; }
        if ('pf' in record) { delete record.pf; modified = true; }
        if (record.out !== temp) {
          record.out = temp;
          modified = true;
        }
        if (!modified) {
          cursor.continue();
          return;
        }
        const updateReq = cursor.update(record);
        updateReq.onsuccess = () => {
          updated++;
          cursor.continue();
        };
        updateReq.onerror = () => {
          console.log('[KNMI] failed to update snapshot', iso);
          cursor.continue();
        };
      };
      cursorReq.onerror = () => resolve(updated);
      tx.oncomplete = () => resolve(updated);
      tx.onabort = () => resolve(updated);
      tx.onerror = () => resolve(updated);
    } catch (_) {
      resolve(updated);
    }
  });
}

function applyCompactionCfg(cfg, fromLoad = false){
  if (!cfg || !Array.isArray(cfg.stages)) return false;
  const stages = cfg.stages.map((st) => {
    const intervalVal = Number(st.targetIntervalSecs ?? st.interval ?? st.step);
    return {
      olderThanSecs: Math.floor(Number(st.olderThanSecs)),
      targetIntervalSecs: Math.floor(intervalVal)
    };
  });
  if (stages.length !== 3) return false;
  if (stages.some(st => !isFinite(st.olderThanSecs) || st.olderThanSecs <= 0 || !isFinite(st.targetIntervalSecs) || st.targetIntervalSecs <= 0)) {
    return false;
  }
  stages.sort((a, b) => a.olderThanSecs - b.olderThanSecs);
  if (!(stages[0].olderThanSecs < stages[1].olderThanSecs && stages[1].olderThanSecs < stages[2].olderThanSecs)) {
    return false;
  }
  compactCfg = { stages };
  compactStatus.error = null;
  if (!fromLoad) {
    persistCompactCfg();
    broadcastAll({ type: 'compactStatus', status: compactStatus, cfg: compactCfg });
  }
  return true;
}

function startCompaction(){
  if (compactRunning) return;
  compactRunning = true;
  compactStop = false;
  compactStatus = {
    running: true,
    phase: 'start',
    progressKey: null,
    startedAt: Date.now(),
    finishedAt: 0,
    stats: { written: 0, deleted: 0 },
    error: null
  };
  broadcastAll({ type: 'compactStatus', status: compactStatus, cfg: compactCfg });
  runCompaction().then(() => {
    compactStatus.running = false;
    compactStatus.phase = 'idle';
    compactStatus.progressKey = null;
    compactStatus.finishedAt = Date.now();
    broadcastAll({ type: 'compactStatus', status: compactStatus, cfg: compactCfg });
  }).catch((err) => {
    compactStatus.running = false;
    compactStatus.phase = 'idle';
    compactStatus.error = err ? String(err) : 'Compactie mislukt';
    compactStatus.progressKey = null;
    compactStatus.finishedAt = Date.now();
    broadcastAll({ type: 'compactStatus', status: compactStatus, cfg: compactCfg });
  }).finally(() => {
    compactRunning = false;
  });
}

async function runCompaction(){
  if (!dbReady || !db) return;
  const nowSec = Math.floor(Date.now()/1000);
  const oldest = await findOldestKey();
  if (typeof oldest !== 'number' || oldest <= 0) return;
  const stagesAsc = compactCfg.stages.slice().sort((a, b) => a.olderThanSecs - b.olderThanSecs);
  const stagesDesc = stagesAsc.slice().reverse();
  if (!stagesDesc.length) return;

  let currentKey = Math.max(1, stagesDesc[0].targetIntervalSecs * Math.floor(oldest / stagesDesc[0].targetIntervalSecs));

  for (const stage of stagesDesc) {
    const interval = Math.max(1, stage.targetIntervalSecs | 0);
    const half = Math.max(1, Math.floor(interval / 2));
    const limit = nowSec - stage.olderThanSecs;
    const stageIdx = stagesAsc.indexOf(stage) + 1;
    compactStatus.phase = `stage${stageIdx}`;
    if (limit <= 1) { currentKey = Math.max(currentKey, limit); continue; }
    let key = Math.max(currentKey, interval * Math.floor(currentKey / interval));
    if (key <= 0) key = interval;

    for (; key < limit; key += interval) {
      if (compactStop) return;
      if (key <= 0) continue;
      compactStatus.progressKey = key;
      const changed = await compactBucket(key, half);
      if (changed) broadcastProgressThrottled();
    }
    currentKey = Math.max(key, limit);
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
        const c = e.target.result;
        if (c) {
          const val = c.value;
          if (val && typeof val.time === 'number' && val.time > 0) {
            out.push(val);
          }
          c.continue();
        } else {
          resolve(out);
        }
      };
    } catch (_) { resolve(out); }
  });
}

function idbDeleteRange(low, high){
  return new Promise((resolve) => {
    try {
      const l = Math.max(1, Math.floor(Math.min(low, high)));
      const h = Math.max(1, Math.floor(Math.max(low, high)));
      if (l > h) { resolve(); return; }
      const tx = db.transaction(DB_STORE_NAME, 'readwrite');
      tx.objectStore(DB_STORE_NAME).delete(IDBKeyRange.bound(l, h));
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
        const cur = e.target.result;
        if (!cur) { resolve(undefined); return; }
        if (typeof cur.key === 'number' && cur.key > 0) {
          resolve(cur.key);
        } else {
          cur.continue();
        }
      };
    } catch (_) { resolve(undefined); }
  });
}

async function compactBucket(centerSec, halfWindowSec){
  if (centerSec <= 0) return false;
  const buf = await idbRange(centerSec - halfWindowSec, centerSec + halfWindowSec);
  if (!buf || buf.length <= 1) return false;
  const mid = Math.floor(buf.length/2);
  const rec = averageRecords(buf, mid);
  rec.time = centerSec;
  const ok = await idbPut(rec);
  if (ok){
    compactStatus.stats.written += 1;
    compactStatus.stats.deleted += Math.max(0, buf.length - 1);
    await idbDeleteRange(centerSec - halfWindowSec, centerSec - 1);
    await idbDeleteRange(centerSec + 1, centerSec + halfWindowSec);
  }
  return ok;
}

function meanOf(arr, sel, fallback){
  let s=0, n=0; for (const x of arr){ const v = sel(x); if (typeof v === 'number' && isFinite(v)) { s+=v; n++; } }
  if (n) return s/n;
  if (typeof fallback === 'number' && isFinite(fallback)) return fallback;
  return undefined;
}
function averageRecords(buf, mid){
  const m = buf[mid] || {};
  const anyOne = buf.some(d => d && d.one_pwr);
  const anyTwo = buf.some(d => d && d.two_pwr);
  const allSafe = buf.every(d => !d || d.safe !== false);
  const allBlue = buf.every(d => !d || d.blue !== false);
  return {
    target: meanOf(buf, d=>d.target, m.target),
    fnt: meanOf(buf, d=>d.fnt, m.fnt),
    bck: meanOf(buf, d=>d.bck, m.bck),
    top: meanOf(buf, d=>d.top, m.top),
    bot: meanOf(buf, d=>d.bot, m.bot),
    chip: meanOf(buf, d=>d.chip, m.chip),
    rem: meanOf(buf, d=>d.rem, m.rem),
    out: meanOf(buf, d=>d.out, m.out),
    voltage: meanOf(buf, d=>d.voltage, m.voltage),
    current: meanOf(buf, d=>d.current, m.current),
    power: meanOf(buf, d=>d.power, m.power),
    energy: m.energy,
    one_pwr: anyOne,
    two_pwr: anyTwo,
    safe: allSafe,
    blue: allBlue
  };
}
