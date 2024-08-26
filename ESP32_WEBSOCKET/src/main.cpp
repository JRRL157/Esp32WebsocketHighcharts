#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <SD.h>
#include <SPI.h>
#include <HX711.h>
#include <string.h>
#include <stdlib.h>

#define BUFFER_SIZE    1024
#define LOAD_CELL_DOUT GPIO_NUM_33
#define LOAD_CELL_SCK  GPIO_NUM_32
#define LOAD_CELL_GAIN 64
#define CS             GPIO_NUM_5
#define MISO		   GPIO_NUM_19
#define MOSI           GPIO_NUM_23
#define CLK            GPIO_NUM_18
#define LED_CONNECTED  GPIO_NUM_27  /* LED VERDE  */
#define LED_CALIBRATE  GPIO_NUM_26  /* LED BRANCO */
#define LED_READING    GPIO_NUM_25  /* LED AZUL   */

const IPAddress apIP = IPAddress(192,168,4,1);
const IPAddress mask = IPAddress(255,255,255,0);

const char* ssid = "ESP32SSID";
const char* password = "12345678#!";

typedef struct {
	uint16_t timeout = 10000;
	float scale = 22216.59;
	float weight = 217.0;
	uint16_t sample_min_limit = 1000;
}config_t;

enum MessageType {
	DATA_ACQUISITION = 1,
	SAMPLING_LIMIT = 2,
	CFG_TIMEOUT = 3,
	CAL_START = 4,
	SD_STATUS = 5,
	VALUE_SEND = 6,
	TIME_SEND = 7,
	CONTINUOUS_READING = 8
};

config_t cfg;
config_t *pCfg = &cfg;
HX711 loadCell;
char timestamp[9];
char sd_status;
File cfg_file;
File log_file;
float value_buffer[BUFFER_SIZE];
uint16_t time_buffer[BUFFER_SIZE];
TaskHandle_t readingTaskHandle = NULL;
TaskHandle_t calibrateTaskHandle = NULL;
TaskHandle_t sendingTaskHandle = NULL;
TaskHandle_t continuousReadingTaskHandle = NULL;

SPIClass spi = SPIClass(VSPI);

/* ========= PROTOTIPOS DE FUNCOES ============ */
void calibrateTask(void *pvParameters);
bool initSDcard(void);
void checkSDconfig(config_t* pcfg);
String readLine(void);
void initLoadCell(void);
void initLittleFS(void);
void initWiFi(void);
void handleBinaryMessage(AsyncWebSocketClient *client, uint8_t *data, size_t len);
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void updateConfigFile(config_t* pcfg);
void initWebSocket(void);
void readingTask(void *pvParameters);
void sendingTask(void *pvParameters);
void continuousReadingTask(void *pvParameters);
void resetDataArray(void);
/* ============================================ */

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create a WebSocket object
AsyncWebSocket ws("/ws");

void calibrateTask(void *pvParameters){
	uint32_t ulEvents;
	char msg[2];
	msg[0] = CAL_START;
	while(1) {
		ulEvents = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		if (ulEvents != 0) {
			
			vTaskSuspend(continuousReadingTaskHandle);

			msg[1] = 1;
			ws.binaryAll(msg, sizeof(msg));
			short n = 5;
			float cal = 0.0;
			loadCell.set_scale();
			loadCell.tare();
			Serial.println("Ponha um peso conhecido na celula");

			while(n--){
				/* pisca o led indicador de calibração por 5 segundos */
				Serial.println(n);
				gpio_set_level(LED_CALIBRATE, 1);
				vTaskDelay(pdMS_TO_TICKS(250));
				gpio_set_level(LED_CALIBRATE, 0);
				vTaskDelay(pdMS_TO_TICKS(750));
			}

			cal = loadCell.get_units(10);
			cfg.scale = cal / cfg.weight;
			loadCell.set_scale(cfg.scale);
			Serial.print("Nova escala: ");
			Serial.println(cfg.scale);
			Serial.println("Calibrado!");
			msg[1] = 0;
			ws.binaryAll(msg, sizeof(msg));
			Serial.print("Mensagem enviada para o websocket: ");
			Serial.println(msg);

			vTaskResume(continuousReadingTaskHandle);
			ulEvents--;
		}			 
	}
}

bool initSDcard(void){

	char msg[2];
	msg[0] = SD_STATUS;

	if (!SD.begin(CS, spi, 10000000)) {
		sd_status = 0;
    	Serial.println("Falha na inicializacao.");
		msg[1] = 0;
		ws.binaryAll(msg, sizeof(msg));
    	return false;
	}
	else{
		sd_status = 1;
		Serial.println("Cartao SD inicializado.");
		msg[1] = 1;
		ws.binaryAll(msg, sizeof(msg));
	}
	return true;
}

String readLine(void){
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
		String temp;
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
		return;
	}		
}

void initLoadCell(void){
	loadCell.begin(LOAD_CELL_DOUT, LOAD_CELL_SCK, LOAD_CELL_GAIN);
	loadCell.set_scale(cfg.scale);
	loadCell.tare();
	Serial.println("Celula de carga iniciada.\n");
}

/* Initialize LittleFS */
void initLittleFS(void) {
	if (!LittleFS.begin(true)) {
    	Serial.println("An error has occurred while mounting LittleFS");
  	}
  	Serial.println("LittleFS mounted successfully");
}

/* Initialize WiFi */
void initWiFi(void) {

	WiFi.mode(WIFI_AP);
	WiFi.softAPConfig(apIP,apIP,mask);

	Serial.print("Starting WiFi Access Point..");
	while (!WiFi.softAP(ssid,password)) {
    	Serial.print('.');
    	vTaskDelay(pdMS_TO_TICKS(1000));
  	}
}

void updateConfigFile(config_t* pcfg){

	cfg_file = SD.open("/config.txt", FILE_WRITE);  /* cria um novo com os dados atualizados */
	if(cfg_file){
		cfg_file.println((String)pcfg->scale);
		cfg_file.println((String)pcfg->timeout);
		cfg_file.println((String)pcfg->weight);
		cfg_file.println((String)pcfg->sample_min_limit);
		Serial.println("Arquivo de configuracao atualizado.");
		cfg_file.close();
	}
	else{
		Serial.println("Erro ao atualizar arquivo de configuracao!");
	}
}

void handleBinaryMessage(AsyncWebSocketClient *client, uint8_t *data, size_t len) {
    
	uint8_t messageType = data[0];                                 // O primeiro byte é o tipo de mensagem
    
	if ((messageType == DATA_ACQUISITION) && (len >= 9)) {         // CODIGO 1
        Serial.println("Salvando timestamp");
		memcpy(timestamp, &data[1], 8);                            // Copiar os 8 bytes da string
        Serial.println("Fechando timestamp");
		timestamp[8] = '\0';                                       // Garantir que a string esteja terminada em null
		Serial.println("Msg de start recebida. Iniciando funcao de leitura.");
		Serial.print("Timestamp salvo: ");
		Serial.println(timestamp);
        xTaskNotifyGive(readingTaskHandle);                        // Inicia leitura
    }
	else if ((messageType == SAMPLING_LIMIT) && (len >= 3)) {      // CODIGO 2 (ajuste de sampling_limit)
    	uint16_t number = 0;
    	memcpy(&number, &data[1], sizeof(uint16_t));
		pCfg->sample_min_limit = number;
		Serial.println("Arquivo atualizado com novo valor de sampling_limit.");
		updateConfigFile(pCfg);
	}
	else if ((messageType == CFG_TIMEOUT) && (len >= 3)) {         // CODIGO 3 (ajuste de timeout)
		uint16_t number = 0;
    	memcpy(&number, &data[1], sizeof(uint16_t));
		pCfg->timeout = number;
		Serial.println("Arquivo atualizado com novo valor de timeout");
		updateConfigFile(pCfg);
	}
	else if ((messageType == CAL_START) && (len >= 5)) {           // CODIGO 4 (ajuste de peso e chamada de calibracao)
		float number = 0.0;
    	memcpy(&number, &data[1], sizeof(float));
		pCfg->weight = number / 1000;  /* converte gramas para kg */
		Serial.println(pCfg->weight);
		Serial.println("Arquivo atualizado e funcao de calibracao chamada.");
		updateConfigFile(pCfg);  /* atualiza o arquivo de cnfg */
		Serial.println("Iniciando calibracao");
		xTaskNotifyGive(calibrateTaskHandle);       /* chama a função de calibração */
	}
	else if ((messageType == SD_STATUS) && (len >= 1)) {
		//Serial.println("Recebido requisicao de status do cartao SD.");
		char msg[2];
		msg[0] = SD_STATUS;
		msg[1] = sd_status;
		ws.binaryAll(msg, sizeof(msg));
		Serial.print("Msg sd-status enviada: ");
		Serial.println(msg);
	}
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
	switch (type) {
	case WS_EVT_CONNECT:
		Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
		gpio_set_level(LED_CONNECTED, 1);
		break;
	case WS_EVT_DISCONNECT:
		Serial.printf("WebSocket client #%u disconnected\n", client->id());
		gpio_set_level(LED_CONNECTED, 0);
		break;
	case WS_EVT_DATA:
		{
			AwsFrameInfo *info = (AwsFrameInfo*)arg;
			if (info->opcode == WS_BINARY) {
				handleBinaryMessage(client, data, len);
			}
			break;
		}
	case WS_EVT_PONG:
	case WS_EVT_ERROR:
		break;
	}
}

void initWebSocket(void){
	ws.onEvent(onEvent);
	server.addHandler(&ws);
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
		request->send(LittleFS, "/index.html", "text/html", false);
	});
	server.serveStatic("/", LittleFS, "/");
	/* Inicia servidor */
	server.begin();
	Serial.println("\nWebsocket iniciado.");
}

void readingTask(void *pvParameters) {
	uint32_t ulEvents;
	float reading = 0.0;
	uint16_t time = 0;
	char msg[2];
	/* converte o valor mínimo de leitura para float e para kg */
	float threshold = ((float)cfg.sample_min_limit) / 1000;
  	while (1) {
		ulEvents = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    	if (ulEvents != 0) {

			vTaskSuspend(continuousReadingTaskHandle);
			vTaskSuspend(calibrateTaskHandle);

			TickType_t time_interval = 0;
			String filename = "";
			uint16_t buff_index = 0;
			msg[0] = DATA_ACQUISITION;
			msg[1] = 1;
			ws.binaryAll(msg, sizeof(msg));
			String str_timestamp = String(timestamp);    /* casting to string */

			filename = "/" + str_timestamp + ".csv";        /* arquivo: DDMMHHMM.csv */
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
			TickType_t timeout = pdMS_TO_TICKS(cfg.timeout);

			/* Continua a leitura enquanto startReading for true e dentro do timeout */
			while ((xTaskGetTickCount() - startTime) < timeout) {
				reading = loadCell.get_units(1);
				gpio_set_level(LED_READING, 1);
				if (reading >= threshold) {
					time_interval = xTaskGetTickCount() - startTime;
					time = (uint16_t)time_interval;
					Serial.print("Time: ");
					Serial.print(time);
					Serial.print(" ## Reading: ");
					Serial.println(reading);
					/* salva a leitura no cartao sd */
					if (log_file) {
						log_file.print(reading);
						log_file.print(",");
						log_file.println(time);
					}
					/* Envia para o buffer */
					if (buff_index <= BUFFER_SIZE) {   /* garante que não haja estouro de buffer */
						value_buffer[buff_index] = reading;
						time_buffer[buff_index] = time;
						buff_index++;
					}
					vTaskDelay(pdMS_TO_TICKS(12U));
				}
				else{
					/* Aguarda 12ms */
					vTaskDelay(pdMS_TO_TICKS(12U));
				}
			}
			gpio_set_level(LED_READING, 0);
			/* Fecha o arquivo */
			log_file.close();
			buff_index = 0;
			msg[1] = 0;
			Serial.println("Amostragem finalizada.");
			ws.binaryAll(msg, sizeof(msg));

			Serial.println("(readingTask) Task de envio notificada.");
			xTaskNotifyGive(sendingTaskHandle);
		}
		ulEvents--;
	}
}

void resetDataArray(void) {
	int i;
    for (i = 0; i < BUFFER_SIZE; i++) {
        value_buffer[i] = 0.0f;
        time_buffer[i] = 0;
    }
	Serial.println("Dados do buffer resetados.");
}

void sendingTask(void *pvParameters){
	/* Essa task entrará em ação quando a task de aquisição de dados estiver finalizada */
	uint32_t ulEvents;
	char value_msg;
	char time_msg;
	value_msg = VALUE_SEND;
	time_msg = TIME_SEND;
  	while (1) {
		ulEvents = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    	if (ulEvents != 0) {
			Serial.println("(SendTask) Notificacao recebida.");
			/* Mensagem indicando inicio de envio de dados de peso */
			ws.binaryAll(&value_msg, sizeof(value_msg));
			/* Envio dos dados de peso */
			ws.binaryAll((uint8_t *)value_buffer, sizeof(value_buffer));

			vTaskDelay(pdMS_TO_TICKS(100));
			/* Mensagem indicando inicio de envio de dados de tempo */
			ws.binaryAll(&time_msg, sizeof(time_msg));
			/* Envio dos dados de tempo */
			ws.binaryAll((uint8_t *)time_buffer, sizeof(time_buffer));

			resetDataArray();
			vTaskResume(continuousReadingTaskHandle);
			vTaskResume(calibrateTaskHandle);
			Serial.println("(SendTask) Task finalizada.");
		}
		ulEvents--;
	}
}

void continuousReadingTask(void *pvParameters){
	float reading;
	char msg;
	msg = CONTINUOUS_READING;
  	while (1) {
		//Serial.println("(ContinuousTask) Enter task.");
		/* Mensagem indicando envio de dado de leitura da célula de carga */
		reading = loadCell.get_units(10);
		Serial.print("Leitura: ");
		Serial.println(reading, 4);
		ws.binaryAll(&msg, sizeof(msg));
		/* Envio do dado de peso */
		ws.binaryAll((uint8_t *)&reading, sizeof(reading));
		//Serial.println("(ContinuousTask) Dado enviado.");
		vTaskDelay(pdMS_TO_TICKS(1000));  // Executa a cada 1 segundo
    }
}

void setup() {
	gpio_set_direction(CS, GPIO_MODE_OUTPUT);
	gpio_set_level(CS, 0);
	gpio_set_direction(LED_CONNECTED, GPIO_MODE_OUTPUT);
	gpio_set_level(LED_CONNECTED, 0);
	gpio_set_direction(LED_CALIBRATE, GPIO_MODE_OUTPUT);
	gpio_set_level(LED_CALIBRATE, 0);
	gpio_set_direction(LED_READING, GPIO_MODE_OUTPUT);
	gpio_set_level(LED_READING, 0);
	spi.begin(CLK, MISO, MOSI, CS);
	Serial.begin(115200);
	initLittleFS();
	initWiFi();
	initWebSocket();
	checkSDconfig(pCfg);
	initLoadCell();
	xTaskCreatePinnedToCore(readingTask, "readingTask", 8192, NULL, 2, &readingTaskHandle, 0);
	xTaskCreatePinnedToCore(calibrateTask, "calibrateTask", 8192, NULL, 2, &calibrateTaskHandle, 0);
	xTaskCreatePinnedToCore(sendingTask, "sendingTask", 8192, NULL, 2, &sendingTaskHandle, 0);
	xTaskCreatePinnedToCore(continuousReadingTask, "continuousReadingTask", 8192, NULL, 2, &continuousReadingTaskHandle, 0);
}

void loop() {
	ws.cleanupClients();
	vTaskDelay(pdMS_TO_TICKS(1000));
}