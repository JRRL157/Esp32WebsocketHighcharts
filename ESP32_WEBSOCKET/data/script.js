var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
// Init web socket when the page loads
window.addEventListener('load', onload);

function onload(event) {
  initWebSocket();
  initButton();
}

function getReadings() {
  websocket.send("getReadings");
}

function initWebSocket() {
  console.log('Trying to open a WebSocket connection…');
  websocket = new WebSocket(gateway);
  websocket.onopen = onOpen;
  websocket.onclose = onClose;
  websocket.onmessage = onMessage;
}

function initButton() {
  document.getElementById('button').addEventListener('click', start);
}

function start() {
  // const timeoutField = document.getElementById('timeout');
  websocket.send('1');
}

// When websocket is established, call the getReadings() function
function onOpen(event) {
  console.log('Connection opened');
  //getReadings();
}

function onClose(event) {
  console.log('Connection closed');
  setTimeout(initWebSocket, 2000);
}

// Create Temperature Chart
var chartT = new Highcharts.Chart({
  chart: {
    renderTo: 'chart-teste-estatico'
  },
  series: [
    {
      name: 'Force',
      type: 'line',
      color: '#00A6A6',
      marker: {
        symbol: 'square',
        radius: 3,
        fillColor: '#00A6A6',
      }
    },
  ],
  title: {
    text: undefined
  },
  xAxis: {
    type: 'datetime',
    dateTimeLabelFormats: { millisecond: '%M:%S.%L' }
  },
  yAxis: {
    title: {
      text: 'Y Axis'
    }
  },
  credits: {
    enabled: false
  }
});

//Plot temperature in the temperature chart
function plotGraph(jsonValue) {

  var keys = Object.keys(jsonValue);
  console.log(keys);
  console.log(keys.length);

  for (var i = 1; i < keys.length; i++) {
    var timeKey = keys[0];
    var t = Number(jsonValue[timeKey]);
    console.log(t);

    const key = keys[i];
    var y = Number(jsonValue[key]);
    console.log(y);

    if (chartT.series[i - 1].data.length > 40) {
      chartT.series[i - 1].addPoint([t, y], true, true, true);
    } else {
      chartT.series[i - 1].addPoint([t, y], true, false, true);
    }
  }
}

// Function that receives the message from the ESP32 with the readings
function onMessage(event) {
  console.log(event.data);
  const button = document.getElementById("button");

  if (event.data != undefined && event.data === "1") {
    console.log("Botão acionado com sucesso!");
    getReadings();
    button.disabled = true;
  } else if (event.data != undefined && event.data === "0") {
    console.log("Teste finalizado!");
    button.disabled = false;
  } else if (event.data != undefined && event.data != "1" && event.data != "0") {
    var strArr = event.data.split(",");
    var time = parseInt(strArr[0],16);
    var force = parseInt(strArr[1],16);

    console.log(time," ",force);
    if(!isNaN(time) && !isNaN(force)){
      console.log("ENTROU AQUI!");
      var jsonObj = { time: time, force: force };
      button.disabled = true;
      plotGraph(jsonObj);
    }
  }
}