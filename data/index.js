// DOM refs
const indicator1 = document.getElementById("indicator1");
const indicator2 = document.getElementById("indicator2");

const inc2 = document.getElementById("increment2");
const inc = document.getElementById("increment");
const target = document.getElementById("input");
const dec = document.getElementById("decrement");
const dec2 = document.getElementById("decrement2");
const overrideClear = document.getElementById("override_clear");

const ti_day = document.getElementById("day_time");
const ta_day = document.getElementById("day_target");
const ti_nig = document.getElementById("night_time");
const ta_nig = document.getElementById("night_target");
const ti_flg = document.getElementById("night_check");

const scheduledTarget = document.getElementById("scheduled_target");
const scheduleMode = document.getElementById("schedule_mode");
const scheduleNext = document.getElementById("schedule_next");
const overrideState = document.getElementById("override_state");

const te_rem = document.getElementById("te_rem");
const te_fnt = document.getElementById("te_fnt");
const te_bck = document.getElementById("te_bck");
const te_top = document.getElementById("te_top");
const te_bot = document.getElementById("te_bot");
const te_chp = document.getElementById("te_chp");

const volt = document.getElementById("volt");
const amp = document.getElementById("amp");
const watt = document.getElementById("watt");
const kwh = document.getElementById("kwh");
const pf = document.getElementById("pf");

const wifi = document.getElementById("wifi");
const blue = document.getElementById("blue");

const timeEl = document.getElementById("time");
const statusLine = document.getElementById("status_line");
const offlineBanner = document.getElementById("offline_banner");
const toast = document.getElementById("toast");
const themeToggle = document.getElementById("themeToggle");
const spark = document.getElementById("spark_rem");

var gateway = `ws://${window.location.hostname}/ws`;
//var gateway = `ws://heater.local/ws`;
var websocket;
var update;

// reconnect backoff (1s -> 2s -> 5s capped)
const backoffSteps = [1000, 2000, 5000, 5000, 5000];
let backoffIndex = 0;

// Coalesced rendering to <=10Hz
let pending = null;
let rafPending = false;
let lastFrame = 0;
let lastSparkTs = 0;

function sleep(ms) {
	return new Promise(resolve => setTimeout(resolve, ms));
}

function initWebSocket() {
	console.log('Trying to open a WebSocket connection...');
	try {
		websocket = new WebSocket(gateway);
		websocket.onopen = onOpen;
		websocket.onclose = onClose;
		websocket.onmessage = onMessage;
	} catch (_) {
		scheduleReconnect();
	}
}

function onOpen(event) {
	console.log('Connection opened');
	wifi.style.background = '#00c4fa';
	if (offlineBanner) offlineBanner.hidden = true;
	backoffIndex = 0;
	setStatus('Verbonden');
}

function onClose(event) {
	console.log('Connection closed');
	wifi.style.background = '#BEBEBE';
	if (offlineBanner) offlineBanner.hidden = false;
	setStatus('Offline – opnieuw verbinden…');
	scheduleReconnect();
}
function scheduleReconnect() {
	const delay = backoffSteps[Math.min(backoffIndex++, backoffSteps.length - 1)];
	setTimeout(initWebSocket, delay);
}
var testarray = [];
var counter = 0;

function onMessage(event) {
	update = JSON.parse(event.data);
	addSnapshot(update);
	pending = update;
	scheduleRender();
}

window.addEventListener('load', onLoad);

function onLoad(event) {
	initWebSocket();
	initButtons();
	ta_nig.disabled = !ti_flg.checked;
	ti_nig.disabled = !ti_flg.checked;
	initTheme();
	openDb();
	sleep(5000).then(() => {
		countRecords();
		oldRecord();
	});
	sleep(10000).then(() => { reduceData(); drawSpark(); });
}

function initButtons() {
	document.getElementById('decrement').addEventListener('click', decrement);
	document.getElementById('decrement2').addEventListener('click', decrement2);
	document.getElementById('increment').addEventListener('click', increment);
	document.getElementById('increment2').addEventListener('click', increment2);
	document.getElementById('day_time').addEventListener('change', day_night);
	document.getElementById('day_target').addEventListener('change', day_night);
	document.getElementById('night_time').addEventListener('change', day_night);
	document.getElementById('night_target').addEventListener('change', day_night);
	document.getElementById('night_check').addEventListener('change', day_night);
	overrideClear.addEventListener('click', clearOverride);
	const qaB30 = document.getElementById('qa_boost30'); if (qaB30) qaB30.addEventListener('click', () => quickBoost(30));
	const qaB60 = document.getElementById('qa_boost60'); if (qaB60) qaB60.addEventListener('click', () => quickBoost(60));
	const qaEco = document.getElementById('qa_eco'); if (qaEco) qaEco.addEventListener('click', () => { websocket.send('D'); showToast('Eco −0,1°'); });
	const qaRes = document.getElementById('qa_resume'); if (qaRes) qaRes.addEventListener('click', clearOverride);
	if (themeToggle) themeToggle.addEventListener('click', toggleTheme);
}

function decrement() {
	websocket.send('D');
}

function decrement2() {
	websocket.send('E');
}

function increment() {
	websocket.send('U');
}

function increment2() {
	websocket.send('V');
}

function day_night() {
ta_nig.disabled = !ti_flg.checked;
ti_nig.disabled = !ti_flg.checked;
const payload = {
	type: 'schedule',
	day_start: ti_day.value,
	night_start: ti_nig.value,
	day_temp: parseFloat(ta_day.value),
	night_temp: parseFloat(ta_nig.value),
	night_enabled: ti_flg.checked
};
if (Number.isNaN(payload.day_temp)) {
	delete payload.day_temp;
}
if (Number.isNaN(payload.night_temp)) {
	delete payload.night_temp;
}
try { websocket.send(JSON.stringify(payload)); showToast('Opgeslagen'); } catch (_) { showToast('Mislukt', true); }
};

function clearOverride() {
	try { websocket.send('R'); showToast('Schema hervat'); } catch (_) { showToast('Mislukt', true); }
}

function formatMinutes(minutes) {
	if (typeof minutes !== 'number' || minutes < 0) {
		return '--';
	}
	if (minutes >= 1440) {
		return '>24u';
	}
	const hrs = Math.floor(minutes / 60);
	const mins = minutes % 60;
	if (hrs === 0) {
		return `${mins} min`;
	}
	return `${hrs}u ${mins.toString().padStart(2, '0')}m`;
}

const DB_NAME = 'Snapshots';
const DB_VERSION = 1;
const DB_STORE_NAME = 'snapshots';

var db;

function openDb() {
	console.log("openDb ...");
	var request = indexedDB.open(DB_NAME, DB_VERSION);

	request.onsuccess = function (evt) {
		// Equal to: db = request.result;
		db = this.result;
		console.log("openDb DONE");
	};

	request.onerror = function (evt) {
		console.error("openDb:", evt.target.errorCode);
	};

	request.onupgradeneeded = function (evt) {
		console.log("openDb.onupgradeneeded");
		var store = evt.currentTarget.result.createObjectStore(DB_STORE_NAME, { keyPath: 'time' });
	};
}

function getObjectStore(store_name, mode) {
	var tx = db.transaction(store_name, mode);
	return tx.objectStore(store_name);
}

function addSnapshot(obj) {
	//	console.log("addSnapshot arguments:", arguments);

	var store = getObjectStore(DB_STORE_NAME, 'readwrite');
	var req;

	try {
		req = store.put(obj);
	} catch (e) {
		throw e;
	}
	req.onsuccess = function (evt) {
		//		console.log("Insertion in DB successful");
	};
	req.onerror = function () {
		console.error("addSnapshot error", this.error);
	};
}

const count = document.getElementById("count");
const old = document.getElementById("old");

function countRecords() {
	const transaction = db.transaction([DB_STORE_NAME], "readonly");
	const objectStore = transaction.objectStore(DB_STORE_NAME);

	const countRequest = objectStore.count();
	countRequest.onsuccess = () => {
		count.textContent = countRequest.result;
	};
}

function oldRecord() {
	const transaction = db.transaction([DB_STORE_NAME], "readonly");
	const objectStore = transaction.objectStore(DB_STORE_NAME);

	objectStore.openCursor(null, 'next').onsuccess = function (event) {
		if (event.target.result) {
			const oldDate = new Date(event.target.result.value.time * 1000);
			old.textContent = oldDate.toLocaleString();
		}
	};
}

var recordBuffer = [];
var tempRecord = {};

async function reduceData() {

	// Find the oldest key in the database
	let result = findOldestKey();
	const oldkey = await result;

	// calculate round minute
	let key = 60 * 10 * Math.floor(oldkey / (60 * 10));

	// Initialize single record object
	result = getRecord(oldkey);
	await result;
	if (tempRecord.length == 0) {
		return;
	}

	while (key < getTimestampInSeconds() - 2 * 24 * 60 * 60) {
		// fill buffer
		result = getRecords(key - 299, key + 300);
		await result;

		if (recordBuffer.length > 1) {
			const middleObject = Math.floor(recordBuffer.length / 2);
			averageBuffer(middleObject);
			tempRecord.time = key;

			// save the record
			try {
				result = putRecord();
				await result;
			} catch (error) { }

			// delete before
			result = deleteRecords(key - 299, key - 1);
			await result;
			// delete after
			result = deleteRecords(key + 1, key + 300);
			await result;
		}
		key += 600;
	}
	console.log("reduceData(): key:", new Date(key * 1000).toLocaleString());
	console.log("reduceData(): step 1 ready");

	while (key < getTimestampInSeconds() - 1 * 60 * 60) {
		// fill buffer
		result = getMinute(key);
		await result;

		if (recordBuffer.length > 0) {
			//console.log("getMinute returned");
			//console.log("reduceData(): key:", key, new Date(key * 1000).toLocaleString());
			//console.log("reduceData(): recordBuffer", recordBuffer);
			//console.log("reduceData(): recordBuffer.length", recordBuffer.length);
			//recordBuffer.forEach(detail => {
			//console.log(new Date(detail.time * 1000).toLocaleString());
			//});
			//console.log("reduceData():temprecord", tempRecord);

			if (recordBuffer.length > 1) {
				//console.log("reduceData(): key:", key, new Date(key * 1000).toLocaleString());
				// Get middle of array
				const middleObject = Math.floor(recordBuffer.length / 2);
				averageBuffer(middleObject);
				tempRecord.time = key;
				//console.log("reduceData():temprecord averedged", tempRecord);

				// save the record
				try {
					result = putRecord();
					await result;
				} catch (error) { }

				// delete before
				result = deleteRecords(key - 29, key - 1);
				await result;
				// delete after
				result = deleteRecords(key + 1, key + 30);
				await result;
				//await sleep(100);
			}
		}
		key += 60;
	}
	console.log("reduceData(): key:", new Date(key * 1000).toLocaleString());
	console.log("reduceData(): ready");
}

function findOldestKey() {
	const transaction = db.transaction([DB_STORE_NAME], "readonly");
	const objectStore = transaction.objectStore(DB_STORE_NAME);
	const cursorRequest = objectStore.openKeyCursor(null, 'next');
	return new Promise((resolve, reject) => {
		cursorRequest.onsuccess = (event) => {
			resolve(event.target.result.key);
		}
		cursorRequest.onerror = (event) => {
			reject(event.target.result.key);
		}
	});
}

function getRecord(key) {
	const transaction = db.transaction([DB_STORE_NAME], "readonly");
	const objectStore = transaction.objectStore(DB_STORE_NAME);
	const getRequest = objectStore.get(key);
	return new Promise((resolve, reject) => {
		getRequest.onsuccess = (event) => {
			resolve(tempRecord = event.target.result);
		}
		getRequest.onerror = (event) => {
			reject(-1);
		}
	});
}

function getMinute(key) {
	const low = key - 29;
	const high = key + 30;
	recordBuffer = [];
	const boundKeyRange = IDBKeyRange.bound(low, high);
	const transaction = db.transaction([DB_STORE_NAME], "readonly");
	const objectStore = transaction.objectStore(DB_STORE_NAME);
	const getRequest = objectStore.openCursor(boundKeyRange);
	//console.log("getMinute()", boundKeyRange);
	return new Promise((resolve, reject) => {
		getRequest.onsuccess = (event) => {
			const cursor = event.target.result;
			if (cursor) {
				recordBuffer.push(cursor.value);
				cursor.continue();
			} else {
				resolve();
			}
		}
		getRequest.onerror = (event) => {
			reject(-1);
		};
	});
}

function getRecords(low, high) {
	recordBuffer = [];
	const boundKeyRange = IDBKeyRange.bound(low, high);
	const transaction = db.transaction([DB_STORE_NAME], "readonly");
	const objectStore = transaction.objectStore(DB_STORE_NAME);
	const getRequest = objectStore.openCursor(boundKeyRange);
	return new Promise((resolve, reject) => {
		getRequest.onsuccess = (event) => {
			const cursor = event.target.result;
			if (cursor) {
				recordBuffer.push(cursor.value);
				cursor.continue();
			} else {
				resolve();
			}
		}
		getRequest.onerror = (event) => {
			reject(-1);
		};
	});
}

function averageBuffer(middle) {
	tempRecord.target = d3.mean(recordBuffer, d => d.target);
	tempRecord.fnt = d3.mean(recordBuffer, d => d.fnt);
	tempRecord.bck = d3.mean(recordBuffer, d => d.bck);
	tempRecord.top = d3.mean(recordBuffer, d => d.top);
	tempRecord.bot = d3.mean(recordBuffer, d => d.bot);
	tempRecord.chip = d3.mean(recordBuffer, d => d.chip);
	//let string = '';
	//recordBuffer.forEach(element => {
	//string += element.rem + ' ';	
	//});
	//console.log("averageBuffer(): rem", string);
	//console.log("averageBuffer(): avg", d3.mean(recordBuffer, d => d.rem));
	tempRecord.rem = d3.mean(recordBuffer, d => d.rem);
	tempRecord.voltage = d3.mean(recordBuffer, d => d.voltage);
	tempRecord.current = d3.mean(recordBuffer, d => d.current);
	tempRecord.bot = d3.mean(recordBuffer, d => d.bot);
	tempRecord.power = d3.mean(recordBuffer, d => d.power);
	tempRecord.energy = recordBuffer[middle].energy;
	tempRecord.pf = recordBuffer[middle].pf;
	tempRecord.one_pwr = recordBuffer[middle].one_pwr;
	tempRecord.two_pwr = recordBuffer[middle].two_pwr;
	tempRecord.safe = recordBuffer[middle].safe;
	tempRecord.blue = recordBuffer[middle].blue;
}

function checkRecord(key) {
	const transaction = db.transaction([DB_STORE_NAME], "readonly");
	const objectStore = transaction.objectStore(DB_STORE_NAME);
	const keyRequest = objectStore.getKey(key);
	//console.log("checkRecord(): key", key);

	return new Promise((resolve, reject) => {
		keyRequest.onsuccess = (event) => {
			//console.log("checkRecord(): event", event);
			resolve(keyRequest.result);
		}
		keyRequest.onerror = (event) => {
			console.log("checkRecord(): event", event);
			reject(keyRequest.result);
		}
	});
}

function putRecord() {
	const transaction = db.transaction([DB_STORE_NAME], "readwrite");
	const objectStore = transaction.objectStore(DB_STORE_NAME);
	const putRequest = objectStore.put(tempRecord);
	//console.log("putRecord():");
	return new Promise((resolve, reject) => {
		putRequest.onsuccess = (event) => {
			//console.log("putRecord(): onsuccess", event);
			resolve(event);
		}
		putRequest.onerror = (event) => {
			console.log("putRecord(): onerror", event);
			reject(event);
		}
	});
}

function deleteRecords(low, high) {
	const boundKeyRange = IDBKeyRange.bound(low, high);
	const transaction = db.transaction([DB_STORE_NAME], "readwrite");
	const deleteRequest = transaction.objectStore(DB_STORE_NAME).delete(boundKeyRange);
	return new Promise((resolve, reject) => {
		deleteRequest.oncomplete = () => {
			//console.log("deleteRecords(): oncomplete");
			resolve();
		};
		deleteRequest.onsuccess = () => {
			//console.log("deleteRecords(): onsuccess");
			resolve();
		};
		deleteRequest.onerror = () => {
			console.log("deleteRecords(): onerror");
			reject();
		};
	});
}









/*
function retrieve(){
//	console.log("Retieve data");
	const high = getTimestampInSeconds();
	const low = high - 3*60 // 24*60*60;
	const boundKeyRange = IDBKeyRange.bound(low, high, false, true);
//	console.log("boundKeyRange", boundKeyRange);

	const transaction = db.transaction([DB_STORE_NAME], "readonly");
	const objectStore = transaction.objectStore(DB_STORE_NAME);
 
	objectStore.openCursor(boundKeyRange).onsuccess = (event) => {
		const cursor = event.target.result;
		var record;
		if (cursor) {
//			plot.push(cursor.value);
			record = cursor.value;
			delete record.target;
			delete record.fnt;
			delete record.bck;
			delete record.top;
			delete record.bot;
			delete record.chip;
			delete record.voltage;
			delete record.current;
			delete record.power;
			delete record.energy;
			delete record.pf;
			delete record.one_pwr;
//			delete record.safe;
			delete record.pf;
			delete record.blue;
			plot.push(record);
			cursor.continue();
		}
//		console.log(record);
//		console.log(record.time);
//		console.log(record.rem);
//		delete record.fnt;
//		delete record.bck;
//		delete record.top;
//		delete record.bot;
//		delete record.chip;
//		delete record.voltage;
//		delete record.current;
//		delete record.power;
//		delete record.energy;
//		delete record.pf;
//		delete record.one_pwr;
//		delete record two_pwr;
//		delete record.safe;
//		delete record.pf;
//		delete record.blue;
//		console.log(record);
//		plot.push(record);
}
	console.log("plot", plot);
//	updateData()
}
*/


function getTimestampInSeconds() {
  return Math.floor(Date.now() / 1000)
}

// UI helpers
function setStatus(text) { if (statusLine) statusLine.textContent = text; }

function scheduleRender() {
  if (rafPending) return;
  rafPending = true;
  requestAnimationFrame(ts => {
    rafPending = false;
    if (ts - lastFrame < 100) return scheduleRender();
    lastFrame = ts;
    if (!pending) return;
    applySnapshot(pending);
    if (ts - lastSparkTs > 30000) { lastSparkTs = ts; drawSpark(); }
  });
}

function applySnapshot(update) {
  if (!update) return;
  te_rem.textContent = update.rem.toFixed(1);
  te_fnt.textContent = update.fnt.toFixed(1);
  te_bck.textContent = update.bck.toFixed(1);
  te_top.textContent = update.top.toFixed(1);
  te_bot.textContent = update.bot.toFixed(1);
  te_chp.textContent = update.chip.toFixed(1);
  target.textContent = update.target.toFixed(1);
  volt.textContent = update.voltage.toFixed(1);
  amp.textContent = update.current.toFixed(3);
  watt.textContent = update.power.toFixed(1);
  kwh.textContent = update.energy.toFixed(2);
  if (pf) pf.textContent = update.pf.toFixed(3);

  indicator1.className = update.one_pwr ? 'stage-pill indicator-on' : 'stage-pill indicator-off';
  indicator2.className = update.two_pwr ? 'stage-pill indicator-on' : 'stage-pill indicator-off';
  blue.style.background = update.blue ? '#00c4fa' : '#6d7e90';

  const d = (typeof update.time === 'number') ? new Date(update.time * 1000) : new Date();
  let text = d.toLocaleTimeString('nl-NL', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
  timeEl.textContent = text;

  if (update.config) {
    if (update.config.day_start && document.activeElement !== ti_day) {
      ti_day.value = update.config.day_start;
    }
    if (update.config.night_start && document.activeElement !== ti_nig) {
      ti_nig.value = update.config.night_start;
    }
    if (typeof update.config.day_temp === 'number' && document.activeElement !== ta_day) {
      ta_day.value = update.config.day_temp.toFixed(1);
    }
    if (typeof update.config.night_temp === 'number' && document.activeElement !== ta_nig) {
      ta_nig.value = update.config.night_temp.toFixed(1);
    }
    if (typeof update.config.night_enabled === 'boolean') {
      ti_flg.checked = update.config.night_enabled;
      ta_nig.disabled = !ti_flg.checked;
      ti_nig.disabled = !ti_flg.checked;
    }
  }

  if (update.schedule) {
    if (typeof update.schedule.target === 'number') {
      scheduledTarget.textContent = update.schedule.target.toFixed(1);
    }
    let modeLabel = 'Dag';
    if (update.schedule.preheat) {
      modeLabel = 'Voorverwarmen';
    } else if (!update.schedule.is_day) {
      modeLabel = 'Nacht';
    }
    scheduleMode.textContent = modeLabel;
    if (typeof update.schedule.minutes_to_next === 'number') {
      scheduleNext.textContent = formatMinutes(update.schedule.minutes_to_next);
    }
    if (update.schedule.override) {
      // Localized override text (Dutch)
      let overrideText = 'Handmatig';
      if (typeof update.schedule.override_target === 'number') {
        overrideText = `Handmatig ${update.schedule.override_target.toFixed(1)} °C`;
      }
      if (update.schedule.override_until) {
        const until = new Date(update.schedule.override_until * 1000);
        overrideText += ` tot ${until.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}`;
      }
      overrideState.textContent = overrideText;
    } else {
      overrideState.textContent = 'Uit';
    }
  }
  countRecords();
}

// Sparkline (last 60 minutes). Shows Room and Target.
function drawSpark() {
  if (!spark || !db) return;
  const ctx = spark.getContext('2d');
  const w = spark.width, h = spark.height;
  ctx.clearRect(0,0,w,h);
  const high = getTimestampInSeconds();
  const low = high - 60*60; // 60 min
  const range = IDBKeyRange.bound(low, high, false, true);
  const tx = db.transaction(['snapshots'], 'readonly');
  const store = tx.objectStore('snapshots');
  const req = store.openCursor(range);
  const pts = [];
  req.onsuccess = (e) => {
    const c = e.target.result;
    if (c) { pts.push({ t: c.value.time * 1000, rem: c.value.rem, tgt: c.value.target }); c.continue(); }
    else {
      if (pts.length < 2) return;
      pts.sort((a,b)=>a.t-b.t);
      const t0 = pts[0].t, t1 = pts[pts.length-1].t;
      // bounds across both series
      let vMin = Infinity, vMax = -Infinity;
      for (const p of pts) { vMin = Math.min(vMin, p.rem, p.tgt); vMax = Math.max(vMax, p.rem, p.tgt); }
      if (!(isFinite(vMin)&&isFinite(vMax)) || vMin===vMax) { vMin=18; vMax=22; }
      // baseline
      ctx.strokeStyle = '#233040'; ctx.lineWidth = 1; ctx.beginPath(); ctx.moveTo(4, h-4); ctx.lineTo(w-4, h-4); ctx.stroke();
      // room
      ctx.strokeStyle = '#ff7b7b'; ctx.lineWidth = 2; ctx.beginPath();
      pts.forEach((p,i)=>{
        const x = (p.t - t0) / Math.max(1, t1 - t0) * (w-8) + 4;
        const y = h - ((p.rem - vMin) / Math.max(0.001,(vMax - vMin))) * (h-8) - 4;
        if (i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y);
      });
      ctx.stroke();
      // target
      ctx.strokeStyle = '#d0a0ff'; ctx.lineWidth = 2; ctx.beginPath();
      pts.forEach((p,i)=>{
        const x = (p.t - t0) / Math.max(1, t1 - t0) * (w-8) + 4;
        const y = h - ((p.tgt - vMin) / Math.max(0.001,(vMax - vMin))) * (h-8) - 4;
        if (i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y);
      });
      ctx.stroke();
    }
  };
}

// Quick boost shortcuts – requires firmware support for duration; here we just nudge target to ensure hold
function quickBoost(mins) {
  try {
    websocket.send('U'); // nudge up 0.1 and extend override window in firmware
    showToast(`Boost ${mins}m (vereist FW)`);
  } catch (_) { showToast('Mislukt', true); }
}

// Theme handling
function initTheme() {
  const saved = localStorage.getItem('theme');
  if (saved === 'dark' || (!saved && window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches)) {
    document.documentElement.setAttribute('data-theme','dark');
    themeToggle && themeToggle.setAttribute('aria-pressed','true');
  }
}
function toggleTheme() {
  const dark = document.documentElement.getAttribute('data-theme') === 'dark';
  if (dark) {
    document.documentElement.removeAttribute('data-theme');
    localStorage.setItem('theme','light');
    themeToggle && themeToggle.setAttribute('aria-pressed','false');
  } else {
    document.documentElement.setAttribute('data-theme','dark');
    localStorage.setItem('theme','dark');
    themeToggle && themeToggle.setAttribute('aria-pressed','true');
  }
}

// Toasts
let toastTimer;
function showToast(text, error=false) {
  if (!toast) return;
  toast.textContent = text;
  toast.style.background = error ? '#5c2121' : '#1b2a3a';
  toast.style.border = '1px solid #2f3b48';
  toast.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(()=>{ toast.hidden = true; }, 1600);
}
