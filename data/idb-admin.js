const DB_NAME = 'Snapshots';
const DB_VERSION = 1;
const STORE_NAME = 'snapshots';

const rowsBody = document.getElementById('snapshot_rows');
const detailTimeEl = document.getElementById('detail_time');
const detailJsonEl = document.getElementById('detail_json');
const saveBtn = document.getElementById('save_btn');
const deleteBtn = document.getElementById('delete_btn');
const detailStatus = document.getElementById('detail_status');
const toastEl = document.getElementById('toast');

const limitInput = document.getElementById('limit_input');
const fromInput = document.getElementById('from_input');
const loadLatestBtn = document.getElementById('load_latest');
const loadRangeBtn = document.getElementById('load_range');
const refreshBtn = document.getElementById('refresh_btn');
const searchInput = document.getElementById('search_input');
const searchBtn = document.getElementById('search_btn');

const statCountEl = document.getElementById('stat_count');
const statOldestEl = document.getElementById('stat_oldest');
const statNewestEl = document.getElementById('stat_newest');
const dbStatusEl = document.getElementById('db_status');
const swStatusEl = document.getElementById('sw_status');

let db = null;
let currentRows = [];
let selectedKey = null;
let swPort = null;
let lastQuery = { mode: 'latest', limit: 50, from: null };

detailJsonEl.value = '';
detailJsonEl.disabled = true;
saveBtn.disabled = true;
deleteBtn.disabled = true;

function showToast(text, error = false) {
  if (!toastEl) return;
  toastEl.textContent = text;
  toastEl.style.background = error ? '#5c2121' : '#1b2a3a';
  toastEl.style.border = '1px solid #2f3b48';
  toastEl.hidden = false;
  setTimeout(() => { toastEl.hidden = true; }, 1800);
}

function fmtTime(epoch) {
  if (!Number.isFinite(epoch)) return '-';
  const iso = new Date(epoch * 1000).toISOString();
  return iso.slice(0, 19).replace('T', ' ');
}

function updateDbStatus(text) {
  if (dbStatusEl) dbStatusEl.textContent = text;
}

function updateSwStatus(text) {
  if (swStatusEl) swStatusEl.textContent = text;
}

function connectSharedWorker() {
  if (!('SharedWorker' in window)) {
    updateSwStatus('geen');
    return;
  }
  try {
    const sw = new SharedWorker('ws-worker.js');
    swPort = sw.port;
    swPort.onmessage = onWorkerMessage;
    swPort.start();
    swPort.postMessage({ type: 'init', host: window.location.hostname, subscribe: false });
    updateSwStatus('opstarten…');
  } catch (err) {
    console.warn('[IDB]', 'SharedWorker init failed', err);
    updateSwStatus('fout');
  }
}

function onWorkerMessage(ev) {
  const msg = ev.data || {};
  if (msg.type === 'ws') {
    updateSwStatus(msg.state === 'open' ? 'verbonden' : 'offline');
  } else if (msg.type === 'ready') {
    updateSwStatus(msg.ws === 'open' ? 'verbonden' : 'offline');
  }
}

function openDatabase() {
  return new Promise((resolve, reject) => {
    try {
      const req = indexedDB.open(DB_NAME, DB_VERSION);
      req.onsuccess = () => {
        db = req.result;
        updateDbStatus('open');
        resolve(db);
      };
      req.onerror = () => {
        updateDbStatus('fout');
        reject(req.error);
      };
    } catch (error) {
      updateDbStatus('fout');
      reject(error);
    }
  });
}

function withStore(mode, fn) {
  if (!db) throw new Error('DB not open');
  const tx = db.transaction(STORE_NAME, mode);
  const store = tx.objectStore(STORE_NAME);
  return fn(store, tx);
}

function requestToPromise(req) {
  return new Promise((resolve, reject) => {
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error);
  });
}

async function loadStats() {
  if (!db) return;
  try {
    const [count, oldest, newest] = await Promise.all([
      withStore('readonly', store => requestToPromise(store.count())),
      withStore('readonly', store => new Promise((resolve) => {
        const req = store.openKeyCursor();
        req.onsuccess = (ev) => {
          const cursor = ev.target.result;
          resolve(cursor && typeof cursor.key === 'number' ? cursor.key : null);
        };
        req.onerror = () => resolve(null);
      })),
      withStore('readonly', store => new Promise((resolve) => {
        const req = store.openKeyCursor(null, 'prev');
        req.onsuccess = (ev) => {
          const cursor = ev.target.result;
          resolve(cursor && typeof cursor.key === 'number' ? cursor.key : null);
        };
        req.onerror = () => resolve(null);
      }))
    ]);
    statCountEl.textContent = count ?? '-';
    statOldestEl.textContent = oldest ? fmtTime(oldest) : '-';
    statNewestEl.textContent = newest ? fmtTime(newest) : '-';
  } catch (_) {
    statCountEl.textContent = statOldestEl.textContent = statNewestEl.textContent = '–';
  }
}

function renderRows(rows) {
  currentRows = rows;
  rowsBody.innerHTML = '';
  for (const row of rows) {
    const tr = document.createElement('tr');
    tr.dataset.key = row.time;
    tr.innerHTML = `
      <td class="mono">${fmtTime(row.time)}</td>
      <td>${formatTemp(row.rem)}</td>
      <td>${formatTemp(row.out)}</td>
      <td>${formatTemp(row.target)}</td>
      <td>${formatNumber(row.power, 1)}</td>
    `;
    tr.addEventListener('click', () => selectRow(row.time));
    rowsBody.appendChild(tr);
  }
  if (!rows.length) {
    const tr = document.createElement('tr');
    tr.innerHTML = `<td colspan="5" style="text-align:center; color:var(--muted); padding:18px;">Geen snapshots in dit bereik</td>`;
    rowsBody.appendChild(tr);
  }
}

function formatTemp(value) {
  return (typeof value === 'number' && Number.isFinite(value)) ? value.toFixed(1) : '–';
}

function formatNumber(value, digits = 2) {
  return (typeof value === 'number' && Number.isFinite(value)) ? value.toFixed(digits) : '–';
}

async function loadLatest(limit) {
  lastQuery = { mode: 'latest', limit, from: null };
  const rows = [];
  try {
    await withStore('readonly', (store) => new Promise((resolve, reject) => {
      let count = 0;
      const req = store.openCursor(null, 'prev');
      req.onsuccess = (ev) => {
        const cursor = ev.target.result;
        if (!cursor || count >= limit) {
          resolve();
          return;
        }
        rows.push(cursor.value);
        count++;
        cursor.continue();
      };
      req.onerror = () => reject(req.error);
    }));
  } catch (err) {
    showToast('Laden mislukt', true);
  }
  renderRows(rows);
}

async function loadRange(from, limit) {
  lastQuery = { mode: 'range', limit, from };
  const rows = [];
  try {
    await withStore('readonly', (store) => new Promise((resolve, reject) => {
      let count = 0;
      const req = store.openCursor(IDBKeyRange.lowerBound(from), 'next');
      req.onsuccess = (ev) => {
        const cursor = ev.target.result;
        if (!cursor || count >= limit) {
          resolve();
          return;
        }
        rows.push(cursor.value);
        count++;
        cursor.continue();
      };
      req.onerror = () => reject(req.error);
    }));
  } catch (err) {
    showToast('Laden mislukt', true);
  }
  renderRows(rows);
}

function selectRow(key) {
  selectedKey = key;
  for (const tr of rowsBody.querySelectorAll('tr')) {
    tr.classList.toggle('selected', Number(tr.dataset.key) === key);
  }
  const row = currentRows.find(r => r.time === key);
  if (!row) {
    detailTimeEl.textContent = '-';
    detailJsonEl.value = '';
    detailJsonEl.disabled = true;
    saveBtn.disabled = true;
    deleteBtn.disabled = true;
    return;
  }
  detailTimeEl.textContent = fmtTime(row.time) + ` (${row.time})`;
  detailJsonEl.disabled = false;
  detailJsonEl.value = JSON.stringify(row, null, 2);
  saveBtn.disabled = false;
  deleteBtn.disabled = false;
  detailStatus.textContent = '';
}

async function searchSnapshots(query) {
  const limit = Math.max(1, Number(limitInput.value) || 1);
  const criteria = parseSearchQuery(query);
  lastQuery = { mode: 'search', limit, query };
  const rows = [];
  try {
    await withStore('readonly', (store) => new Promise((resolve, reject) => {
      const req = store.openCursor();
      req.onsuccess = (ev) => {
        const cursor = ev.target.result;
        if (!cursor || rows.length >= limit) {
          resolve();
          return;
        }
        const value = cursor.value;
        if (matchesCriteria(value, criteria)) {
          rows.push(value);
        }
        cursor.continue();
      };
      req.onerror = () => reject(req.error);
    }));
  } catch (err) {
    showToast('Zoeken mislukt', true);
  }
  renderRows(rows);
}

async function handleSave() {
  if (selectedKey === null || !detailJsonEl.value.trim()) return;
  let data;
  try {
    data = JSON.parse(detailJsonEl.value);
  } catch (err) {
    detailStatus.textContent = 'JSON ongeldig';
    return;
  }
  if (typeof data.time !== 'number' || !Number.isFinite(data.time)) {
    detailStatus.textContent = 'Field "time" moet een numerieke epoch (seconden) zijn';
    return;
  }
  try {
    await withStore('readwrite', (store, tx) => {
      store.put(data);
      return new Promise((resolve, reject) => {
        tx.oncomplete = resolve;
        tx.onerror = () => reject(tx.error);
        tx.onabort = () => reject(tx.error);
      });
    });
    detailStatus.textContent = 'Opgeslagen';
    showToast('Snapshot opgeslagen');
    await refreshLastQuery();
  } catch (err) {
    detailStatus.textContent = 'Opslaan mislukt';
  }
}

async function handleDelete() {
  if (selectedKey === null) return;
  if (!confirm('Snapshot verwijderen?')) return;
  try {
    await withStore('readwrite', (store, tx) => {
      store.delete(selectedKey);
      return new Promise((resolve, reject) => {
        tx.oncomplete = resolve;
        tx.onerror = () => reject(tx.error);
        tx.onabort = () => reject(tx.error);
      });
    });
    showToast('Snapshot verwijderd');
    selectedKey = null;
    detailTimeEl.textContent = '-';
    detailJsonEl.value = '';
    detailJsonEl.disabled = true;
    saveBtn.disabled = true;
    deleteBtn.disabled = true;
    await refreshLastQuery();
  } catch (err) {
    detailStatus.textContent = 'Verwijderen mislukt';
  }
}

async function refreshLastQuery() {
  const limit = Math.max(1, Number(limitInput.value) || 1);
  if (lastQuery.mode === 'latest') {
    await loadLatest(limit);
  } else if (lastQuery.mode === 'range' && Number.isFinite(lastQuery.from)) {
    await loadRange(lastQuery.from, limit);
  } else if (lastQuery.mode === 'search' && lastQuery.query) {
    await searchSnapshots(lastQuery.query);
  }
  await loadStats();
}

function bindEvents() {
  loadLatestBtn.addEventListener('click', () => {
    const limit = Math.max(1, Number(limitInput.value) || 1);
    loadLatest(limit).then(loadStats);
  });
  loadRangeBtn.addEventListener('click', () => {
    const limit = Math.max(1, Number(limitInput.value) || 1);
    const from = parseDateTimeLocal(fromInput.value);
    if (!Number.isFinite(from)) {
      showToast('Voer een geldige datum/tijd in', true);
      return;
    }
    loadRange(from, limit).then(loadStats);
  });
  refreshBtn.addEventListener('click', () => {
    refreshLastQuery();
  });
  saveBtn.addEventListener('click', handleSave);
  deleteBtn.addEventListener('click', handleDelete);
  searchBtn.addEventListener('click', () => {
    const q = (searchInput.value || '').trim();
    if (!q) {
      refreshLastQuery();
      return;
    }
    searchSnapshots(q).then(loadStats);
  });
  searchInput.addEventListener('keydown', (ev) => {
    if (ev.key === 'Enter') {
      ev.preventDefault();
      const q = (searchInput.value || '').trim();
      if (!q) refreshLastQuery();
      else searchSnapshots(q).then(loadStats);
    }
  });
}

function parseDateTimeLocal(value) {
  if (!value) return NaN;
  const ms = Date.parse(value);
  if (!Number.isFinite(ms)) return NaN;
  return Math.round(ms / 1000);
}

function parseSearchQuery(input) {
  const text = (input || '').trim();
  if (!text) return { type: 'text', needle: '' };
  const fieldMatch = text.match(/^([a-zA-Z0-9_\.]+)\s*(=|!=|>=|<=|>|<)\s*(.+)$/);
  if (!fieldMatch) {
    return { type: 'text', needle: text.toLowerCase() };
  }
  const [, fieldRaw, op, rawValue] = fieldMatch;
  const field = fieldRaw.trim();
  const value = parseSearchValue(field, rawValue.trim());
  return { type: 'field', field, op, value, raw: rawValue.trim() };
}

function parseSearchValue(field, raw) {
  if (!raw.length) return raw;
  if ((raw.startsWith('"') && raw.endsWith('"')) || (raw.startsWith("'") && raw.endsWith("'"))) {
    return raw.slice(1, -1);
  }
  if (raw.toLowerCase() === 'true') return true;
  if (raw.toLowerCase() === 'false') return false;
  if (raw.toLowerCase() === 'null') return null;
  const num = Number(raw);
  if (Number.isFinite(num)) return num;
  if (field === 'time' || field.endsWith('_time')) {
    const epoch = Date.parse(raw);
    if (Number.isFinite(epoch)) return Math.round(epoch / 1000);
  }
  return raw;
}

function matchesCriteria(record, criteria) {
  if (!criteria) return true;
  if (criteria.type === 'text') {
    if (!criteria.needle) return true;
    const blob = JSON.stringify(record).toLowerCase();
    return blob.includes(criteria.needle);
  }
  if (criteria.type === 'field') {
    const value = getFieldValue(record, criteria.field);
    return compareValue(value, criteria.op, criteria.value, criteria.raw);
  }
  return true;
}

function getFieldValue(record, path) {
  const parts = path.split('.');
  let cur = record;
  for (const part of parts) {
    if (cur && Object.prototype.hasOwnProperty.call(cur, part)) {
      cur = cur[part];
    } else {
      return undefined;
    }
  }
  return cur;
}

function asComparable(value) {
  if (typeof value === 'number') return value;
  if (typeof value === 'boolean') return value ? 1 : 0;
  if (typeof value === 'string') {
    const num = Number(value);
    if (Number.isFinite(num)) return num;
    return value.toLowerCase();
  }
  return value;
}

function compareValue(actual, op, expected, raw) {
  if (actual === undefined) return false;
  const aComp = asComparable(actual);
  let bComp = expected;
  if (typeof expected === 'number' && typeof aComp !== 'number') {
    const coerced = Number(actual);
    if (Number.isFinite(coerced)) {
      bComp = expected;
      return compareNumbers(coerced, op, expected);
    }
  }
  if (typeof aComp === 'number' && typeof expected === 'number') {
    return compareNumbers(aComp, op, expected);
  }
  if (typeof aComp === 'string' && typeof expected === 'string') {
    return compareStrings(aComp, op, expected.toLowerCase());
  }
  if (typeof expected === 'string' && typeof aComp === 'string') {
    return compareStrings(aComp, op, expected.toLowerCase());
  }
  if (typeof expected === 'boolean' && typeof actual === 'boolean') {
    return compareBooleans(actual, op, expected);
  }
  if ((expected === null && actual === null) || (expected === null && actual === undefined)) {
    return op === '=' || op === '==';
  }
  const blob = JSON.stringify(actual).toLowerCase();
  if (op === '=' || op === '==') {
    return blob === JSON.stringify(expected).toLowerCase();
  }
  if (op === '!=' || op === '<>') {
    return blob !== JSON.stringify(expected).toLowerCase();
  }
  return false;
}

function compareNumbers(a, op, b) {
  switch (op) {
    case '=':
    case '==': return Math.abs(a - b) < 1e-6;
    case '!=':
    case '<>': return Math.abs(a - b) >= 1e-6;
    case '>': return a > b;
    case '>=': return a >= b || Math.abs(a - b) < 1e-6;
    case '<': return a < b;
    case '<=': return a <= b || Math.abs(a - b) < 1e-6;
    default: return false;
  }
}

function compareStrings(a, op, b) {
  const aLower = a.toLowerCase();
  switch (op) {
    case '=':
    case '==': return aLower === b.toLowerCase();
    case '!=':
    case '<>': return aLower !== b.toLowerCase();
    case '>': return aLower > b.toLowerCase();
    case '>=': return aLower >= b.toLowerCase();
    case '<': return aLower < b.toLowerCase();
    case '<=': return aLower <= b.toLowerCase();
    default: return false;
  }
}

function compareBooleans(a, op, b) {
  switch (op) {
    case '=':
    case '==': return a === b;
    case '!=':
    case '<>': return a !== b;
    default: return false;
  }
}

async function init() {
  bindEvents();
  connectSharedWorker();
  try {
    await openDatabase();
    await loadLatest(Math.max(1, Number(limitInput.value) || 1));
    await loadStats();
  } catch (err) {
    console.error('[IDB] init failed', err);
    showToast('Database openen mislukt', true);
  }
}

init();
