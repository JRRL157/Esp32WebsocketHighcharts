/*********
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete instructions at https://RandomNerdTutorials.com/esp32-websocket-server-sensor/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <queue>

#include <string.h>
#include <stdlib.h>

#define MAX_PARAMETERS 10

const IPAddress apIP = IPAddress(192,168,4,1);
const IPAddress mask = IPAddress(255,255,255,0);

const char* ssid = "ESP32SSID";
const char* password = "12345678#!";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create a WebSocket object
AsyncWebSocket ws("/ws");

//Parameters to be set by Front End
float params[6];
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

// Initialize LittleFS
void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  Serial.println("LittleFS mounted successfully");
}

// Get Sensor Readings and return char*
void getSensorReadings(){
  unsigned long epoch = LAST_TIME + samp_time;
  unsigned long force = random(100,760);

  snprintf(message,sizeof(message),"%X,%X",epoch,force);
}

// Initialize WiFi
void initWiFi() {

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP,apIP,mask);

  Serial.print("Starting WiFi Access Point..");
  while (!WiFi.softAP(ssid,password)) {
    Serial.print('.');
    delay(1000);
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
    
    Serial.println(cData);
    if (cData[0] == '1') {
      start = true;
      parseParameters(cData, params, &numParamsFound,NULL);

      Serial.print("Received data = ");
      Serial.println((char*)data);
      Serial.print("# parameters found = ");
      Serial.println(numParamsFound);
      
      samp_time = (uint32_t)params[1];
      timeout_time = (uint32_t)params[2];
      scale = params[3];
      prop_mass = params[4];
      weight = params[5];

      Serial.println(samp_time);
      Serial.println(timeout_time);
      Serial.println(scale);
      Serial.println(prop_mass);
      Serial.println(weight);

      startTime = millis();
      Serial.println("Teste Iniciado com sucesso!");
      Serial.println("=============================");
      
    }
    if(start){
      LAST_TIME = millis();
      getSensorReadings();
      Serial.print(message);
      notifyClients();
    }
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

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
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

void setup() {
  Serial.begin(115200);
  initWiFi();
  initLittleFS();
  initWebSocket();

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

  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.serveStatic("/",LittleFS,"/");

  // Start server
  server.begin();
}

void loop() {
}