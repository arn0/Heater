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
