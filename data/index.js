const indicator1 = document.getElementById("indicator1");
const indicator2 = document.getElementById("indicator2");

const inc2 = document.getElementById("increment2");
const inc = document.getElementById("increment");
const target = document.getElementById("input");
const dec = document.getElementById("decrement");
const dec2 = document.getElementById("decrement2");

const ti_day = document.getElementById("day_time");
const ta_day = document.getElementById("day_target");
const ti_nig = document.getElementById("night_time");
const ta_nig = document.getElementById("night_target");
const ti_flg = document.getElementById("night_check");

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
const matt = document.getElementById("matt");

const time = document.getElementById("time");
const container = document.getElementById("container");

var gateway = `ws://${window.location.hostname}/ws`;
//var gateway = `ws://heater.local/ws`;
var websocket;
var update;

function sleep(ms) {
	return new Promise(resolve => setTimeout(resolve, ms));
}

function initWebSocket() {
	console.log('Trying to open a WebSocket connection...');
	websocket = new WebSocket(gateway);
	websocket.onopen = onOpen;
	websocket.onclose = onClose;
	websocket.onmessage = onMessage;
}

function onOpen(event) {
	console.log('Connection opened');
	wifi.style.background = '#00c4fa';
}

function onClose(event) {
	console.log('Connection closed');
	wifi.style.background = '#BEBEBE';
	setTimeout(initWebSocket, 2000);
}
var testarray = [];
var counter = 0;

function onMessage(event) {
	update = JSON.parse(event.data);
	addSnapshot(update);
	//console.log(event.data);
	//console.log(update);
	//testarray.push(update);
	//console.log(new Date(update.time * 1000).toLocaleString());
	//console.log(new Date(testarray[counter++].time * 1000).toLocaleString());

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
	pf.textContent = update.pf.toFixed(3);
	if (update.one_pwr) {
		indicator1.className = 'indicator-on';
	} else {
		indicator1.className = 'indicator-off';
	}
	if (update.two_pwr) {
		indicator2.className = 'indicator-on';
	} else {
		indicator2.className = 'indicator-off';
	}
	if (update.blue) {
		blue.style.background = '#00c4fa';
	} else {
		blue.style.background = '#BEBEBE';
	}
	const d = new Date();
	let text = d.toLocaleString();
	time.textContent = text;
	countRecords();
	//	retrieve();
}

window.addEventListener('load', onLoad);

function onLoad(event) {
	initWebSocket();
	initButtons();
	openDb();
	sleep(5000).then(() => {
		countRecords();
		oldRecord();
	});
	sleep(10000).then(() => { reduceData(); });
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
let message = [];
message.push('check',ti_flg.checked);
message.push('daytime',ti_day.value);
message.push('daytarget',ta_day.value);
message.push('nighttime',ti_nig.value);
message.push('nighttarget',ta_nig.value);
websocket.send(JSON.stringify(message));
};

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
