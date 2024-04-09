const indicator1 = document.getElementById("indicator1");
const indicator2 = document.getElementById("indicator2");

const inc = document.getElementById("increment");
const target = document.getElementById("input");
const dec = document.getElementById("decrement");

const te_rem = document.getElementById("te_rem");
const te_frt = document.getElementById("te_frt");
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

var gateway = `ws://${window.location.hostname}/ws`;
//var gateway = `ws://heater.local/ws`;
var websocket;
var update;

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
	te_frt.textContent = update.env.toFixed(1);
	te_top.textContent = update.top.toFixed(1);
	te_bot.textContent = update.bot.toFixed(1);
	te_chp.textContent = update.chip.toFixed(1);
	target.textContent = update.target.toFixed(1);
	volt.textContent = update.voltage.toFixed(1);
	amp.textContent = update.current.toFixed(1);
	watt.textContent = update.power.toFixed(1);
	kwh.textContent = update.energy.toFixed(1);
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
	const d = new Date();
	let text = d.toLocaleString();
	time.textContent = text;
	
}

window.addEventListener('load', onLoad);

function onLoad(event) {
	initWebSocket();
	initButtons();
	openDb();
}
		 
function initButtons() {
	document.getElementById('decrement').addEventListener('click', decrement);
	document.getElementById('increment').addEventListener('click', increment);
}

function decrement(){
	websocket.send('D');
}

function increment(){
	websocket.send('U');
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
	console.log("addSnapshot arguments:", arguments);

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
		console.log("Insertion in DB successful");
	};
	req.onerror = function() {
		console.error("addSnapshot error", this.error);
	};
}

function retrieve24hr(){
	var history;
	const now = getTimestampInSeconds();
	var search = now - 24*60*60;
	var searching = true;

	while(search < now){
		while(searching){

		}
	}
}

function getTimestampInSeconds () {
  return Math.floor(Date.now() / 1000)
}