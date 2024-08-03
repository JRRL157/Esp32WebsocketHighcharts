var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
var STOP_STR = "000000000\0";
var start_message = "1";

var sample_time_obj = document.getElementById("samp_time");
var timeout_time_obj = document.getElementById("timeout_time");
var start_button_obj = document.getElementById("button");
var prop_mass_obj = document.getElementById("prop_mass");
var scale_obj = document.getElementById("scale");
var weight_obj = document.getElementById("weight");

// Init web socket when the page loads
window.addEventListener('load', onload);

function onload(event) {
  initWebSocket();
  initEventListeners();
}

function getReadings(event) {
  websocket.send("getReadings");
}

function initWebSocket() {
  console.log('Trying to open a WebSocket connection…');
  websocket = new WebSocket(gateway);
  websocket.onopen = onOpen;
  websocket.onclose = onClose;
  websocket.onmessage = onMessage;
}

function initEventListeners() {
  start_button_obj.addEventListener('click', start);
  sample_time_obj.addEventListener('input',samp_time_func);
  timeout_time_obj.addEventListener('input',timeout_time_func);
  prop_mass_obj.addEventListener('input',prop_mass_func);
  scale_obj.addEventListener('input',scale_func);
  weight_obj.addEventListener('input',weight_func);
}

function start() {    
  start_message = "1,"+sample_time_obj.value+","+timeout_time_obj.value+","+scale_obj.value+","+prop_mass_obj.value+","+weight_obj.value;
  websocket.send(start_message);
}

function samp_time_func(){
  console.log("Samp time changed!");
  console.log(sample_time_obj.value);
}

function timeout_time_func(){
  console.log("Timeout time changed!");
  console.log(timeout_time_obj.value);
}

function prop_mass_func(){
  console.log("Timeout time changed!");
  console.log(prop_mass_obj.value);
}
function scale_func(){
  console.log("Timeout time changed!");
  console.log(scale_obj.value);
}
function weight_func(){
  console.log("Timeout time changed!");
  console.log(weight_obj.value);
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

    chartT.series[i-1].addPoint([t,y],true,false,true);
    /*
    if (chartT.series[i - 1].data.length > 40) {
      chartT.series[i - 1].addPoint([t, y], true, true, true);
    } else {
      chartT.series[i - 1].addPoint([t, y], true, false, true);
    }
    */
  }
}

// Function that receives the message from the ESP32 with the readings
function onMessage(event) {
  //console.log("sample_time = ",sample_time_obj.value," timeout_time = ",timeout_time_obj.value);
  
  console.log("[Data] = ",event.data," [Length] = ", Object.keys(event.data).length);
  
  console.log("[1] Finalizou? = ",(event.data === STOP_STR));
  console.log("[2] Finalizou? = ",(event.data == STOP_STR));

  if (event.data != undefined && event.data == start_message) {
    console.log("Botão acionado com sucesso!");
    chartT.series[0] = [];
    chartT.series[1] = [];
    getReadings();
    start_button_obj.disabled = true;
  } else if (event.data != undefined && event.data == STOP_STR) {
    console.log("Teste finalizado!");
    start_button_obj.disabled = false;
  } else if (event.data != undefined && event.data != start_message && event.data != STOP_STR) {
    var strArr = event.data.split(",");
    var time = parseInt(strArr[0],16);
    var force = parseInt(strArr[1],16);
    
    if(!isNaN(time) && !isNaN(force)){
      console.log("ENTROU AQUI!");
      var jsonObj = { time: time, force: force };
      start_button_obj.disabled = true;
      plotGraph(jsonObj);
    }
  }
}