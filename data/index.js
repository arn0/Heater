const indicator1 = document.getElementById("indicator1");
const indicator2 = document.getElementById("indicator2");

const inc2 = document.getElementById("increment2");
const inc = document.getElementById("increment");
const target = document.getElementById("input");
const dec = document.getElementById("decrement");
const dec2 = document.getElementById("decrement2");

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
	websocket.onopen    = onOpen;
	websocket.onclose   = onClose;
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

function onMessage(event) {
	update = JSON.parse(event.data);
	addSnapshot(update);
	//console.log(event.data);
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
	if(update.one_pwr){
		indicator1.className = 'indicator-on';
	}else{
		indicator1.className = 'indicator-off';
	}
	if(update.two_pwr){
		indicator2.className = 'indicator-on';
	}else{
		indicator2.className = 'indicator-off';
	}
	if(update.blue){
		blue.style.background = '#00c4fa';
	}else{
		blue.style.background = '#BEBEBE';
	}
	const d = new Date();
	let text = d.toLocaleString();
	time.textContent = text;
	
//	retrieve();
}

window.addEventListener('load', onLoad);

function onLoad(event) {
	initWebSocket();
	initButtons();
	openDb();
	sleep(5000).then(() => { countRecords(); });
}
		 
function initButtons() {
	document.getElementById('decrement').addEventListener('click', decrement);
	document.getElementById('decrement2').addEventListener('click', decrement2);
	document.getElementById('increment').addEventListener('click', increment);
	document.getElementById('increment2').addEventListener('click', increment2);
}

function decrement(){
	websocket.send('D');
}

function decrement2(){
	websocket.send('E');
}

function increment(){
	websocket.send('U');
}

function increment2(){
	websocket.send('V');
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
		var store = evt.currentTarget.result.createObjectStore(DB_STORE_NAME, { keyPath: 'time'});
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
		req = store.add(obj);
	} catch (e) {
		if (e.name == 'DataCloneError')
			displayActionFailure("This engine doesn't know how to clone a Blob, " +
													 "use Firefox");
		throw e;
	}
	req.onsuccess = function (evt) {
//		console.log("Insertion in DB successful");
	};
	req.onerror = function() {
		console.error("addSnapshot error", this.error);
	};
}

const count = document.getElementById("count");
const old = document.getElementById("old");

function countRecords(){
	const transaction = db.transaction([DB_STORE_NAME], "readonly");
	const objectStore = transaction.objectStore(DB_STORE_NAME);
	
	const countRequest = objectStore.count();
	countRequest.onsuccess = () => {
	  count.textContent = countRequest.result;
	};
	oldRecord();
}

function oldRecord(){
	const transaction = db.transaction([DB_STORE_NAME], "readonly");
	const objectStore = transaction.objectStore(DB_STORE_NAME);
	
	objectStore.openCursor(null,'next').onsuccess = function(event) {
		if (event.target.result) {
			const oldDate = new Date(event.target.result.value.time*1000);
			old.textContent = oldDate.toLocaleString();
		}
	};
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


function getTimestampInSeconds () {
  return Math.floor(Date.now() / 1000)
}
