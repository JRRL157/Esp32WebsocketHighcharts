#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <queue>
#include <SD.h>
#include <HX711.h>

#include <string.h>
#include <stdlib.h>

#define MAX_PARAMETERS 10
#define LOAD_CELL_DOUT GPIO_NUM_32
#define LOAD_CELL_SCK  GPIO_NUM_33
#define CS GPIO_NUM_5

const IPAddress apIP = IPAddress(192,168,4,1);
const IPAddress mask = IPAddress(255,255,255,0);

const char* ssid = "ESP32SSID";
const char* password = "12345678#!";

typedef struct {
	uint16_t timeout = 10000;
	float scale = -268.20;
	float weight = 217.0;	
	uint16_t sample_min_limit = 1000;
}config_t;

typedef struct {
	float value;
	uint16_t time;
}sample_data_t;

config_t cfg;
config_t *pCfg = &cfg;
HX711 loadCell;
String timestamp = "";
File cfg_file;
File log_file;
TaskHandle_t readingTaskHandle = NULL;
TaskHandle_t webSocketSendTaskHandle = NULL;
bool startReading = false;
bool sdStatus = false;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create a WebSocket object
AsyncWebSocket ws("/ws");

//Parameters to be set by Front End
float params[2];
size_t numParamsFound;

// Internal variables gathered from params vector
uint32_t samp_time = 10;
uint32_t timeout_time = 30000;
float scale = -268.20;
float prop_mass = 1000;
float weight = 217.0;

/*
  Runtime Constants

  Warning: WS_TIME_DELAY must be carefully chosen!

*/
uint32_t LAST_TIME = 0;
uint32_t WS_TIME_DELAY = 100;

unsigned long startTime;

TaskHandle_t readSensorTask;
TaskHandle_t notifyTask;

char message[10];

// Holds Started/To Start state
bool start = false;

void calibrate(float weight){
	short n = 5;
	float cal = 0.0;
	loadCell.set_scale();
	loadCell.tare();
	Serial.println("Ponha um peso conhecido na celula");
	while(n--){
		Serial.println(n);
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
	cal = loadCell.get_units(10);
	cfg.scale = cal / weight;
	loadCell.set_scale(cfg.scale);
	Serial.print("Nova escala: ");
    Serial.println(cfg.scale);
	Serial.println("Calibrado!");
}

bool initSDcard(void){
	if (!SD.begin(CS)) {
    	Serial.println("Falha na inicializacao.");
    	return false;
	}
	else{
		Serial.println("Cartao SD inicializado.");
	}
	return true;
}


String readLine(){
	char c;
	String line = "";
	while(true){
		c = cfg_file.read();
    	line = line + c;
    	if(c == '\n'){
      		return line;
    	}
	}
}

void checkSDconfig(config_t* pcfg){
	if(initSDcard()){
		sdStatus = true;
		String temp = "";
		/* checa se existe arquivo de configuração */
		if(SD.exists("/config.txt")){
			cfg_file = SD.open("/config.txt");
			if(cfg_file){
				/* escreve as configurações padrão */
				temp = readLine();
				pcfg->scale = temp.toFloat();
				temp = readLine();
				pcfg->timeout = temp.toInt();
				temp = readLine();
				pcfg->weight = temp.toFloat();
        temp = readLine();
        pcfg->sample_min_limit = temp.toInt();
				Serial.println("Configuracoes feitas!");
				cfg_file.close();
			}
			else{
				Serial.println("Erro ao abrir arquivo de configuracao!");
			}
		}
		else{
			/* se não existir arquivo de cfg, cria um novo */
			cfg_file = SD.open("/config.txt", FILE_WRITE);
			if(cfg_file){
				/* escreve os valores padrão salvos hardcoded nas variaveis globais */
				cfg_file.println((String)pcfg->scale);
				cfg_file.println((String)pcfg->timeout);
				cfg_file.println((String)pcfg->weight);
        cfg_file.println((String)pcfg->sample_min_limit);
				Serial.println("Arquivo de configuracao criado com os valores padrão.");
				cfg_file.close();
			}
			else{
				Serial.println("Erro ao criar arquivo de configuracao!");
			}
		}
	}
	else{
		Serial.println("Falha na configuracao.");
		sdStatus = false;
		return;
	}		
}

void initLoadCell(void){
	loadCell.begin(LOAD_CELL_DOUT, LOAD_CELL_SCK, 128);
	loadCell.set_scale(cfg.scale);
	loadCell.tare();
	Serial.println("Celula de carga iniciada.\n");
}
/* Initialize LittleFS */
void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  Serial.println("LittleFS mounted successfully");
}
/* ============================================================================================== */
// Get Sensor Readings and return char*
void getSensorReadings(){
  unsigned long epoch = LAST_TIME + samp_time;
  unsigned long force = random(100,760);

  snprintf(message,sizeof(message),"%X,%X",epoch,force);
}
/* ============================================================================================== */

// Initialize WiFi
void initWiFi() {

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP,apIP,mask);

  Serial.print("Starting WiFi Access Point..");
  while (!WiFi.softAP(ssid,password)) {
    Serial.print('.');
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void notifyClients() {
  ws.textAll(message,sizeof(message));
}

void parseParameters(const void *data, void *params, size_t *numParams,bool isFloat) {
    //Casting void to a proper type
    const char *pData = (const char*)data;
    float *pParams = (float *)params;
    
    char buffer[256]; // 256 length must be big enough to handle the parameters
    strncpy(buffer, pData, 255); // copies data to buffer variable
    buffer[255] = '\0'; // Make sure it's null-terminated

    /*char* strtok(char* str, const char* delimiters);
     *
      Explanation: 
      
      On a first call, the function expects a C string as argument for str, 
      whose first character is used as the starting location to scan for tokens. 
      In subsequent calls, the function expects a NULL pointer and uses the position 
      right after the end of the last token as the new starting location for scanning.
     
     *
    */

    char *token = strtok(buffer, ",");
    size_t index = 0;

    while (token != NULL && index < MAX_PARAMETERS) {
        //strtoul = (string to unsigned long int)(const char*, char** endptr, int base)
        pParams[index] = strtof(token, NULL); 
        index++;
        token = strtok(NULL, ",");
    }

    *numParams = index; // Updates the number of parameters read    
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    const char *cData = (const char*)data;
    
    parseParameters(cData, params, &numParamsFound,NULL);
	
	Serial.print("Received data = ");
    Serial.println(cData);
    Serial.print("# parameters found = ");
    Serial.println(numParamsFound);	
	
	memset(message, '0', sizeof(message));
    message[9] = '1';
            
	switch((uint8_t)params[0]){
		case 1:
			start = true;
			startTime = (unsigned long)params[1];
      		Serial.println("Teste Iniciado com sucesso!");
      		Serial.println("=============================");
			message[0] = '1';
			break;
		case 2:
		    pCfg->sample_min_limit = (uint16_t)params[1];
			message[0] = '2';
			break;
		case 3:
			pCfg->timeout = (uint16_t)params[1];
			message[0] = '3';
			break;
		case 4:
			pCfg->scale = params[1]; //Is it 'scale' or 'propmass' (????)
			message[0] = '4';
			break;
		case 5:
			pCfg->weight = params[1];
			message[0] = '5';
			break;
	}

	//Send response to Javascript to ensure the operation was done correctly
	notifyClients();
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket(void){
	ws.onEvent(onEvent);
	server.addHandler(&ws);
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
	request->send(LittleFS, "/index.html", "text/html",false);
	});
	server.serveStatic("/", LittleFS, "/");
	/* Inicia servidor */
	server.begin();
	Serial.println("Websocket iniciado.");
}

void readSensorReadingFunc(void *parameter){
  for(;;){
    if(start && (millis() - startTime > timeout_time)){
      start = false;
      memset(message, '0', sizeof(message));
      message[9] = '\0';
      Serial.println("ACABOU!! ACABOU!! ACABOU!!!");
      Serial.println(message);
      notifyClients();
    }
    if (start && (millis() - LAST_TIME) > samp_time) {
      LAST_TIME = millis();
      getSensorReadings();
    }
    ws.cleanupClients();
  }
}

void notifyClientFunc(void *param){
  for(;;){
    notifyClients();
    uint32_t *delayTime = (uint32_t*)(param);
    delay(*delayTime);
  }
}

void readingTask(void *pvParameters) {
	TickType_t time_interval = 0;
	sample_data_t reading;
	String filename = "";

  	while (1) {
    	if (startReading) {
			filename = "/" + timestamp + ".csv";        /* arquivo: DDMMHHMM.csv */
			Serial.println("Amostragem...");
			/* Abre o arquivo no SD */
			Serial.print("Full filename: ");
      Serial.println(filename);
			log_file = SD.open(filename, FILE_WRITE);
			if (log_file){
        Serial.print("Arquivo criado com sucesso: ");
        Serial.println(filename);
      }
			else
				Serial.println("Erro ao criar arquivo.");

			log_file.println("reading,time");

			/* Configura o timeout */
			TickType_t startTime = xTaskGetTickCount();
			const TickType_t timeout = pdMS_TO_TICKS(pCfg->timeout);
		
			/* Continua a leitura enquanto startReading for true e dentro do timeout */
			while (startReading && (xTaskGetTickCount() - startTime) < timeout) {
				reading.value = loadCell.get_units(1);

				if (reading.value >= pCfg->sample_min_limit) {
					time_interval = xTaskGetTickCount() - startTime;
					reading.time = (uint16_t)time_interval;
					Serial.print("Time: ");
          Serial.print(reading.time);
          Serial.print("Reading: ");
          Serial.println(reading.value);
					/* salva a leitura no cartao sd */
					if (log_file) {
						log_file.print(reading.value);
						log_file.print(",");
						log_file.println(reading.time);
					}
					/* Envia para a fila */
					vTaskDelay(pdMS_TO_TICKS(13U));
				}
				else{
					/* Aguarda 13ms */
					vTaskDelay(pdMS_TO_TICKS(13U));
				}
			}
			/* Fecha o arquivo quando startReading for false ou timeout */
			log_file.close();
			startReading = false; /* Reseta a variável startReading após o timeout */
			Serial.println("Amostragem finalizada.");
		}
		else {
			/* Aguarda até que startReading seja true */
			vTaskDelay(pdMS_TO_TICKS(100U));
		}
	}
}

void webSocketSendTask(void *pvParameters) {
	sample_data_t sample;
	String message = "";
  	while (1) {
    	/* Verifica se há dados na fila */
    	vTaskDelay(pdMS_TO_TICKS(100U));  /* envia msgs para o websocket a cada 100ms */
  	}
}

void setup() {
  gpio_set_direction(CS, GPIO_MODE_OUTPUT);
	gpio_set_level(CS, 0);
  Serial.begin(115200);
  initLittleFS();
	checkSDconfig(pCfg);
  initWiFi();
  initWebSocket();
  initLoadCell();
	xTaskCreatePinnedToCore(readingTask, "Reading Task", 8192, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(webSocketSendTask, "WebSocket Send Task", 8192, NULL, 1, NULL, 0);

  xTaskCreatePinnedToCore(
          readSensorReadingFunc,   /* Task function. */
          "readSensorReading",     /* name of task. */
          10000,                   /* Stack size of task */
          NULL,                    /* parameter of the task */
          1,                       /* priority of the task */
          &readSensorTask,         /* Task handle to keep track of created task */
          1                        /* Running at second core */
  );

  xTaskCreatePinnedToCore(
          notifyClientFunc,   /* Task function. */
          "notifyClient",     /* name of task. */
          10000,              /* Stack size of task */
          &WS_TIME_DELAY,     /* parameter of the task */
          1,                  /* priority of the task */
          &notifyTask,        /* Task handle to keep track of created task */
          0                   /* Running at first core */
  );
}
void loop() {

}