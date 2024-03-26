const heat_1 = document.getElementById("button1");
const heat_2 = document.getElementById("button2");

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

const wifi = document.getElementById("wifi");
const blue = document.getElementById("blue");
const matt = document.getElementById("matt");


var gateway = `ws://${window.location.hostname}/ws`;
var websocket;

function initWebSocket() {
 console.log('Trying to open a WebSocket connection...');
 websocket = new WebSocket(gateway);
 websocket.onopen    = onOpen;
 websocket.onclose   = onClose;
 websocket.onmessage = onMessage; // <-- add this line
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

 switch(event.data.at(0)){
  case "r":
   te_rem.innerHTML = event.data.substring(1);
   break;
  case "e":
   te_frt.innerHTML = event.data.substring(1);
   break;
  case "t":
   te_top.innerHTML = event.data.substring(1);
   break;
  case "b":
   te_bot.innerHTML = event.data.substring(1);
   break;
  case "c":
   te_chp.innerHTML = event.data.substring(1);
   break;
  case "a":
   target.value = event.data.substring(1);
   break;
  case "i":
   if(event.data.at(1) == '1') {
    heat_1.className = 'he-button-on';
   }else{
    heat_1.className = 'he-button-off';
   }
   break;
  case "h":
   if(event.data.at(1) == '1') {
    heat_2.className = 'he-button-on';
   }else{
    heat_2.className = 'he-button-off';
   }
   break;
 }
}

window.addEventListener('load', onLoad);

function onLoad(event) {
 initWebSocket();
 initButtons();
}
     
function initButtons() {
 document.getElementById('button1').addEventListener('click', toggle_one);
 document.getElementById('button2').addEventListener('click', toggle_two);
 document.getElementById('decrement').addEventListener('click', decrement);
 document.getElementById('increment').addEventListener('click', increment);
}

function toggle_one(){
 websocket.send('I');
}

function toggle_two(){
 websocket.send('H');
}

function decrement(){
 websocket.send('D');
}

function increment(){
 websocket.send('U');
}
