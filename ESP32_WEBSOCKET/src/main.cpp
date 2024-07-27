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
#include <Arduino_JSON.h>
#include <queue>

const IPAddress apIP = IPAddress(192,168,4,1);
const IPAddress mask = IPAddress(255,255,255,0);

const char* ssid = "ESP32SSID";
const char* password = "12345678#!";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create a WebSocket object
AsyncWebSocket ws("/ws");

// Json Variable to Hold Sensor Readings
JSONVar readings;

// Timer variables
unsigned long LAST_TIME = 0;
unsigned long TIMER_DELAY = 10;
unsigned long TIMEOUT_TIME = 30000;

unsigned long startTime;

TaskHandle_t readSensorTask;
TaskHandle_t notifyTask;

std::queue<String> q;
String sensorReadings;

// Experiment state
bool start = false;


// Initialize LittleFS
void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  Serial.println("LittleFS mounted successfully");
}


// Get Sensor Readings and return JSON object
String getSensorReadings(){
  readings["epoch"] = LAST_TIME + TIMER_DELAY;
  readings["temperature"] = random(16,40);
  readings["humidity"] = random(60,95);
  readings["pressure"] = random(100,760);
  String jsonString = JSON.stringify(readings);
  return jsonString;
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

void notifyClients(String sensorReadings) {
  ws.textAll(sensorReadings);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {        
    if (strcmp((char*)data, "1") == 0) {            
      start = true;      
      startTime = millis();
      Serial.println("Teste Iniciado com sucesso!");
      Serial.print(start);
      Serial.println(startTime);
      Serial.println("=============================");      
    }
    if(start){
      String sensorReadings = getSensorReadings();
      Serial.print(sensorReadings);
      notifyClients(sensorReadings);
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
    if(start && (millis() - startTime > TIMEOUT_TIME)){
      start = false;
      notifyClients("0");
    }
    if (start && (millis() - LAST_TIME) > TIMER_DELAY) {
      LAST_TIME = millis();
      sensorReadings = getSensorReadings();      
    }
    ws.cleanupClients();
  }
}

void notifyClientFunc(void *param){
  for(;;){        
    notifyClients(sensorReadings);
    delay(300); //ERROR: Too many messages queued when delay = 100ms
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
          NULL,               /* parameter of the task */
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