var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
var start_button_obj = document.getElementById("start-button");
var prop_mass;
const valueArray = [];
const timeArray = [];
const gravity = 9.806;

var continuous_reading_on = false;
var value_buffer_receive_on = false;
var time_buffer_receive_on = false;

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

function onOpen(event) {    
	console.log('Connection opened');
	while(websocket.readyState !== WebSocket.OPEN){ 
		/* ESPERA ESTAR CONECTADO */ 
	}
	setInterval(getSdStatus, 20000);
}

function onClose(event) {
	console.log('Connection closed');
	setTimeout(initWebSocket, 1000);
}

function plotGraph() {
	var total = 0;

	for(var j = 0; j < valueArray.length; j++){
		total += valueArray[j];
	}
	var avg = total / valueArray.length;
	var avg_array = [];

	for(var j = 0; j < valueArray.length; j++){
		avg_array[j] = avg;
	}
	console.log("Avg = ", avg);
	console.log("valueArray:");
	console.log(valueArray);
	console.log("timeArray");
	console.log(timeArray);

	Highcharts.chart('container', {
		chart: {
			type: 'line'
		},
		title: {
			text: 'Teste estático'
		},
		subtitle: {
			text: 'Force vs time'
		},
		xAxis: {
			categories: timeArray,
			title: {
            	text: 'Time (ms)'
        	}
		},
		yAxis: {
			title: {
				text: 'Force (N)'
			}
		},
		plotOptions: {
			line: {
				dataLabels: {
					enabled: false
				},
				enableMouseTracking: true,
				lineColor: ' #471cc4 '
			}
		},
		series: [{
			name: 'Força',
			lineWidth: 0.8,
			data: valueArray
		},
		{
			name: 'Força média',
			lineWidth: 1,
			data: avg_array,
			lineColor: ' #FFC300 '
		}]
	});
}

function onMessage(event) {
	const data = new DataView(event.data);
	const messageType = data.getUint8(0); // O primeiro byte é o tipo de mensagem (offset 0)
	// verifica se houve uma mensagem informando que a próxima mensagem será o array de dados
	if (data.byteLength > 2) {
		// entra aqui se o esp32 for enviar o array com os dados do sensor
		if (continuous_reading_on == true) {
			handleContinuousReading(data);
		}
		else if (value_buffer_receive_on == true) {
			console.log("Value buffer received");
			handleValueBufferData(data);
		}
		else if (time_buffer_receive_on == true) {
			console.log("Time buffer received");
			handleTimeBufferData(data);
		}
	}
	else {
		switch (messageType) {
			case 1: 
				console.log("Msg de inicio de leitura recebida");
				handleDataAcquisition(data);
				break;
			case 4:
				console.log("Msg de calibracao recebida");
				handleCalibrateMessage(data);
				break;
			case 5:
				console.log("Msg de sd status recebida");
				handleSdMessage(data);
				break;
			case 6:
				value_buffer_receive_on = true;
				break;
			case 7:
				time_buffer_receive_on = true;
				break;
			case 8:
				continuous_reading_on = true;
				break;
			default:
				console.error('Unknown message type:', messageType);
		}
	}
}

function handleDataAcquisition(dataView) {
	if (dataView.getUint8(1) == 1) {
		document.getElementById("read-info").innerHTML = "Running";
	}
	else {
		document.getElementById("read-info").innerHTML = "Stopped";
	}
	data_acquisition_on = false;
}

function handleContinuousReading(dataView) {
	//console.log("Continuous reading received");
	let reading;
	reading = dataView.getFloat32(0, true).toFixed(2);
	document.getElementById("reading-value").innerHTML = reading;
	//console.log(reading);
	continuous_reading_on = false;
}

function handleValueBufferData(dataView) {
	valueArray.length = 0
	for (let i = 0; i < dataView.byteLength; i += 4) {                   // float(4 bytes)
		let value_str = (dataView.getFloat32(i, true) * gravity).toFixed(2); // little-endian
		let value = parseFloat(value_str);
		// salva os dados no array
		if (value !== 0.00) {
			valueArray.push(value);
			//console.log('Reading Data - Value:', value);
		}
	}
	value_buffer_receive_on = false;
}

function handleTimeBufferData(dataView) {
	timeArray.length = 0
	for (let i = 0; i < dataView.byteLength; i += 2) { // uint16_t(2 bytes)
		let time = (dataView.getUint16(i, true))       // little-endian
		// salva os dados no array
		if (time !== 0) {
			timeArray.push(time);
			//console.log('Reading Data - Time:', time);
		}
	}
	time_buffer_receive_on = false;
	plotGraph();
}

function handleSdMessage(dataView) {
	// pula o primeiro byte, offset = 1
	if (dataView.getUint8(1) == 1) {
		document.getElementById("sd-info").innerHTML = "OK!";
	}
	else {
		document.getElementById("sd-info").innerHTML = "Error!";
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