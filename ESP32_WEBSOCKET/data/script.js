var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
var FINISHED_CODE = "000000001\0";
var START_CODE = "100000001\0";
var LIMIT_CODE = "200000001\0";
var TIMEOUT_CODE = "300000001\0";
var PROPELLENT_CODE = "400000001\0";
var CALIBRATE_CODE = "500000001\0";

var start_message = "1";

var start_button_obj = document.getElementById("start-button");
var prop_mass;

/* 
  ===== key ====== | ======= value =======
  [1, timestamp] -> Start sampling with timestamp
  [2, value] -> sample limit cfg
  [3, value] -> timeout cfg
  [4, value] -> weight (calibrate)
*/

/*
  Response      Description
  0000000001 -> Experiment finished SUCCESS
  1000000001 -> Expriment start SUCCESS
  2000000001 -> sample_limit SET CORRECTLY
  3000000001 -> timeout SET CORRECTLY
  4000000001 -> propellent Mass SET CORRECTLY
  5000000001 -> Calibrate SET CORRECTLY

  7000000001 -> Calibration SUCCESS
  8000000001 -> SD Card detection SUCCESS
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
      //let timestamp = getDate() + getTime();
      let timestamp = Date.now(); //Epoch in miliseconds, this makes things easier to count time in the C/C++ backend
      let message = [1, parseInt(timestamp, 10)];
      console.log(message);
      if(websocket.readyState !== WebSocket.CLOSED){
        websocket.send(message);
      }
    }
  });
}

function sendFormData(formId) {

  if(formId === "form-samplingLimit"){
      var formElements = document.forms[formId].elements['limit'].value;
      console.log("Form sample limit: ", formElements);
      let message = [2, formElements];
      if(websocket.readyState !== WebSocket.CLOSED){
        websocket.send(message);
      }
  }
  else if(formId === "form-timeout"){
      var formElements = document.forms[formId].elements['timeout'].value;
      console.log("Form timeout: ", formElements);
      let message = [3, formElements];
      if(websocket.readyState !== WebSocket.CLOSED){
        websocket.send(message);
      }
  }
  else if(formId === "form-propmass"){
      var formElements = document.forms[formId].elements['propmass'].value;
      prop_mass = formElements;
      console.log("Form prop mass: ", formElements);
      let message = [4, formElements];
      if(websocket.readyState !== WebSocket.CLOSED){
        websocket.send(message);
      }
  }
  else if(formId === "form-weight"){
      var formElements = document.forms[formId].elements['weight'].value;
      console.log("Form weight: ", formElements);
      let message = [5, formElements];
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
  
  console.log("[1] Finalizou? = ",(event.data === FINISHED_CODE));
  console.log("[2] Finalizou? = ",(event.data == FINISHED_CODE));

  if (event.data != undefined && event.data == START_CODE) {
    console.log("Botão acionado com sucesso!");    
    //getReadings();
    start_button_obj.disabled = true;
  } else if (event.data != undefined && event.data == FINISHED_CODE) {
    console.log("Teste finalizado!");
    start_button_obj.disabled = false;
  } else if (event.data != undefined && event.data != START_CODE && event.data != FINISHED_CODE) {
    var strArr = event.data.split(",");
    var time = parseInt(strArr[0],16);
    var force = parseInt(strArr[1],16);
    
    if(!isNaN(time) && !isNaN(force)){
      console.log("ENTROU AQUI!");
      var jsonObj = { time: time, force: force };
      start_button_obj.disabled = true;
      plotGraph(jsonObj);
    }

    //TODO: Criar os outros else if para os demais códigos de retorno
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