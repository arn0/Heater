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
}

function onClose(event) {
  console.log('Connection closed');
  setTimeout(initWebSocket, 2000);
}

function onMessage(event) {
  update = JSON.parse(event.data);
  //console.log(event.data);
}

window.addEventListener('load', onLoad);

function onLoad(event) {
  initWebSocket();
  initButtons();
}
     
function initButtons() {
  document.getElementById('points').addEventListener('click', savePoints);
}

function savePoints(){
  websocket.send('SavePoints');
}
