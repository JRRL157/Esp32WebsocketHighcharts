var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
var start_button_obj = document.getElementById("start-button");
var prop_mass;
const dataArray = [];
const gravity = 9.806;
var buffer_receive_ready = false;

/* 
  ===== key ====== | ======= value =======
  [1, timestamp] -> Start sampling with timestamp
  [2, value] -> sample limit cfg
  [3, value] -> timeout cfg
  [4, value] -> weight (calibrate)
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
  websocket.binaryType = 'arraybuffer';
}

function initEventListeners() {
  start_button_obj.addEventListener("click", function(event){
    
    event.preventDefault();
    if (confirm("Deseja realmente iniciar?")) {
      console.log("Leitura confirmada.");
      let timestamp = getDate() + getTime();
      console.log(timestamp);
      const buffer = new ArrayBuffer(9); // 1byte do código + 8bytes do timestamp
      const dataView = new DataView(buffer);
      dataView.setUint8(0, 1); // Cabeçalho indicando inicio da leitura (1)
      for (let i = 0; i < 8; i++) {
        dataView.setUint8(1 + i, timestamp.charCodeAt(i) || 0); // Codificação da string
      }
      console.log(buffer);
      websocket.send(buffer);
      console.log("Msg enviada.");
    }
  });
}

function getSdStatus() {
  const buffer = new ArrayBuffer(1);       // 1byte do código
  const dataView = new DataView(buffer);
  dataView.setUint8(0, 5);                 // Cabeçalho 
  websocket.send(buffer);                  // Pede informação do cartão SD
  console.log("Msg de requisicao sd status enviada");
}

function sendFormData(formId) {

  if (formId == "form-samplingLimit") {

      var formElements = document.forms[formId].elements['limit'].value;
      console.log("Form sample limit: ", formElements);
      const buffer2 = new ArrayBuffer(3);   // 3bytes -> 1byte (char) + 2bytes (uint16_t)
      const dataView2 = new DataView(buffer2);
      dataView2.setUint8(0, 2);     // primeira posição da mensagem, com código 2
      dataView2.setUint16(1, formElements, true);  // envia, na segunda posição do buffer, o valor do limite mínimo (máx 65535)

      websocket.send(buffer2);

  }
  else if(formId == "form-timeout"){

      var formElements = document.forms[formId].elements['timeout'].value;
      console.log("Form timeout: ", formElements);
      const buffer3 = new ArrayBuffer(3);  // 3bytes -> 1byte (char) + 2bytes (uint16_t)
      const dataView3 = new DataView(buffer3);
      dataView3.setUint8(0, 3);     // primeira posição da mensagem, com código 3 (definir novo timeout)
      dataView3.setUint16(1, formElements, true);  // envia, na segunda posição do buffer, o valor do timeout (máx 65535)
      
      websocket.send(buffer3);
  }
  else if(formId == "form-propmass"){

      var formElements = document.forms[formId].elements['propmass'].value;
      prop_mass = formElements;
      console.log("Form prop mass: ", formElements);
  }
  else if(formId == "form-weight"){

      var formElements = document.forms[formId].elements['weight'].value;
      console.log("Form weight: ", formElements);
      const buffer4 = new ArrayBuffer(5);  // 5 bytes -> 1 byte + 4 bytes (float32)
      const dataView4 = new DataView(buffer4);
      dataView4.setUint8(0, 4);     // primeira posição da mensagem, com código 4 (iniciar calibragem)
      dataView4.setFloat32(1, formElements, true);  // envia, na segunda posição do buffer, o valor do peso de referencia (máx 65535)

      websocket.send(buffer4);
  }
}

// When websocket is established, call the getReadings() function
function onOpen(event) {    
  console.log('Connection opened');
}

function onClose(event) {
  console.log('Connection closed');
  setTimeout(initWebSocket, 1000);
}

// Create Chart
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
      text: 'Force (N)'
    }
  },
  credits: {
    enabled: false
  }
});

function plotGraph(dataArray) {

  const data = dataArray.map(item => [item.time, item.value]);
  chartT.series[0].setData(data);
  
}

function onMessage(event) {
  const data = new DataView(event.data);
  const messageType = data.getUint8(0); // O primeiro byte é o tipo de mensagem (offset 0)

  // verifica se houve uma mensagem informando que a próxima mensagem será o array de dados
  if (buffer_receive_ready == true) {
    // entra aqui se o esp32 for enviar o array com os dados do sensor
    console.log("Inicio de leitura do buffer");
    handleSensorData(data);
  }
  else {
    switch (messageType) {
      case 1: // o esp enviou msg informando que a próxima será o array
          buffer_receive_ready = true;
          console.log("Msg de inicio de leitura recebida");
          break;
      case 5: // SD STATUS
          handleSdMessage(data);
          console.log("Msg de sd status recebida");
          break;
      case 4:
          handleCalibrateMessage(data);
          console.log("Msg de calibracao recebida");
          break;
      default:
          console.error('Unknown message type:', messageType);
    }
  }
}

function handleSensorData(dataView) {
  for (let i = 0; i < dataView.byteLength; i += 6) { // float(4 bytes) + uint16_t(2 bytes) = 6 bytes
      const value = (dataView.getFloat32(i, true) * gravity).toFixed(2); // little-endian
      const time = dataView.getUint16(i + 4, true); // little-endian
      // salva os dados nos arrays
      dataArray.push({ time, value });
      console.log('Reading Data - Value:', value, 'Time:', time);
  }
  plotGraph(dataArray);
  buffer_receive_ready = false;    // reseta a flag
}

function handleSdMessage(dataView) {
  // pula o primeiro byte, offset = 1
  if (dataView.getUint8(1) == 1) {
    document.getElementById("sd-info").innerHTML = "SD Card OK!";
  }
  else {
    document.getElementById("sd-info").innerHTML = "SD Card missing!";
  }
}

function handleCalibrateMessage(dataView) {
  if (dataView.getUint8(1) == 1) {
    document.getElementById("cal-info").innerHTML = "Running";
  }
  else {
    document.getElementById("cal-info").innerHTML = "Stopped";
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

setInterval(getSdStatus, 5000);   // pede informação do cartão SD a cada 5 segundos
getSdStatus();