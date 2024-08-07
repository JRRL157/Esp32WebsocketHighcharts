var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
var STOP_STR = "000000000\0";
var start_message = "1";

var start_button_obj = document.getElementById("start-button");
var prop_mass;

/* 
  ===== key ====== | ======= value =======
  [1, timestamp] -> Start sampling with timestamp
  [2, 1] -> sd status get function (from ESP32)
  [3, value] -> sample limit cfg
  [4, value] -> timeout cfg
  [5, value] -> scale cfg
  [6, value] -> weight cfg
  [7, 1] -> calibrate function start
*/

// Init web socket when the page loads
window.addEventListener('load', onload);

function onload(event) {
  initWebSocket();
  initEventListeners();
}

function initWebSocket() {
  console.log('Trying to open a WebSocket connection…');
  websocket = new WebSocket(gateway);
  websocket.onopen = onOpen;
  websocket.onclose = onClose;
  websocket.onmessage = onMessage;
}

function initEventListeners() {
  start_button_obj.addEventListener('click', function(event){
    event.preventDefault();
    if(confirm("Deseja realmente iniciar?")){
      let timestamp = getDate() + getTime();
      let message = [1, parseInt(timestamp, 10)];
      console.log(message);
      if(websocket.readyState !== WebSocket.CLOSED){
        websocket.send(message);
      }
    }
  });
}

function start() {    
  start_message = "1,"+sample_time_obj.value+","+timeout_time_obj.value+","+scale_obj.value+","+prop_mass_obj.value+","+weight_obj.value;
  websocket.send(start_message);
}

function sendFormData(formId) {

  if(formId === "form-samplingLimit"){
      var formElements = document.forms[formId].elements['limit'].value;
      console.log("Form sample limit: ", formElements);
      let message = [3, formElements];
      if(websocket.readyState !== WebSocket.CLOSED){
        websocket.send(message);
      }
  }
  else if(formId === "form-timeout"){
      var formElements = document.forms[formId].elements['timeout'].value;
      console.log("Form timeout: ", formElements);
      let message = [4, formElements];
      if(websocket.readyState !== WebSocket.CLOSED){
        websocket.send(message);
      }
  }
  else if(formId === "form-scale"){
      var formElements = document.forms[formId].elements['scale'].value;
      console.log("Form scale: ", formElements);
      let message = [5, formElements];
      if(websocket.readyState !== WebSocket.CLOSED){
        websocket.send(message);
      }
  }
  else if(formId === "form-propmass"){
      var formElements = document.forms[formId].elements['propmass'].value;
      prop_mass = formElements;
      console.log("Form prop mass: ", formElements);
  }
  else if(formId === "form-weight"){
      var formElements = document.forms[formId].elements['weight'].value;
      console.log("Form weight: ", formElements);
      let message = [6, formElements];
      if(websocket.readyState !== WebSocket.CLOSED){
        websocket.send(message);
      }
  }
}

// When websocket is established, call the getReadings() function
function onOpen(event) {    
  console.log('Connection opened');

  //getReadings();
}

function onClose(event) {
  console.log('Connection closed');
  setTimeout(initWebSocket, 1000);
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

function getDate(){
  var date = new Date;
  const options = {
  month: 'numeric',
  day: 'numeric',
  };
  return date.toLocaleDateString("pt-Br", options).replace("/", "");
}

function getTime(){
  var date = new Date;
  const options = {
      hour: "numeric",
      minute: "numeric"
      };
      return date.toLocaleTimeString("pt-Br", options).replace(":", "");
}