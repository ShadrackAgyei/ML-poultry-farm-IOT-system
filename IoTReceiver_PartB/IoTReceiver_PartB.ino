/* Receiver Code - Gateway Code */  

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include "html.h"

// nrf24
RF24 radio(4, 5); // CE, CSN

// WiFi Configuration
const char* ssid = "Shaddy001";
const char* password = "1twoeight7";

// Addresses for 4 nodes 
const uint64_t addresses[] = {
  0xA1A1A1A101LL,   // Node 1
  0xA1A1A1A102LL,   // Node 2
  0xA1A1A1A103LL,   // Node 3
  0xA1A1A1A104LL,   // Node 4
};

typedef struct {
  int nodeID;
  float temp;
  float humidity;
  int light;
} sensorData;

typedef struct {
  int targetNode;
  int command;  //0 for off, 1 for on
} ControlCommand;

WebServer server(80);

// stores the latest data for each node
sensorData latestData[5];
unsigned long lastDataTime[5] = {0};
bool nodeActive[5] = {false};

// MQTT Broker
const char* mqtt_server = "172.20.10.2";  // guys if any of you wants to use, change this to your MQTT broker 
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(9600);

  // connects the WiFi
  setupWiFi();

  // connects to the MQTT
  setupMQTT();

  // starts the nrf24 
  radio.begin();
  radio.setAutoAck(false);
  radio.setPALevel(RF24_PA_MIN);

  //opens the 4 reading pipes (pipe 1 to 4)
  radio.openReadingPipe(1, addresses[0]);
  radio.openReadingPipe(2, addresses[1]);
  radio.openReadingPipe(3, addresses[2]);
  radio.openReadingPipe(4, addresses[3]);


  radio.startListening();

  Serial.println("Gateway Ready...");

  // serve the webpage
  server.on("/", []() {
    server.send_P(200, "text/html", MAIN_page);
  });

  // handles the control commands
  server.on("/control", []() {
    if (!server.hasArg("node") || !server.hasArg("cmd")) {
      server.send(400, "text/plain", "Missing parameters");
      return;
    }

    int node = server.arg("node").toInt();
    int cmd  = server.arg("cmd").toInt();

    Serial.println("\n WEB COMMAND RECEIVED ");
    Serial.print("Node: ");
    Serial.println(node);
    Serial.print("Command: ");
    Serial.println(cmd == 1 ? "ON" : "OFF");
    Serial.println("\n\n\n");

    // send commands to the nrf24
    sendCommandToNode(node, cmd);

    // publishs command to MQTT for logging
    StaticJsonDocument<128> doc;
    doc["target"] = node;
    doc["command"] = cmd;
    char buffer[64];
    serializeJson(doc, buffer);

    client.publish("farm/control", buffer);

    server.send(200, "text/plain", "OK");
  });

  server.begin();
  Serial.println("Web Server Started!");
}

void loop() {
  server.handleClient();
  client.loop();
  
  byte pipeNum;
  if (radio.available(&pipeNum)) {
    sensorData data;
    radio.read(&data, sizeof(data));

    // sasve hte latest reading
    int index = data.nodeID - 1;
    latestData[index] = data;
    lastDataTime[index] = millis();
    nodeActive[index] = true;

    Serial.println("\n DATA RECEIVED ");
    Serial.print("From Node: ");
    Serial.println(data.nodeID);
    Serial.print("Temp: ");
    Serial.print(data.temp);
    Serial.println(" °C");
    Serial.print("Humidity: ");
    Serial.print(data.humidity);
    Serial.println(" %");
    Serial.print("Light: ");
    Serial.print(data.light);
    Serial.println(" %");

    // MQTT publish
    char topic[32];
    sprintf(topic, "farm/node%d", data.nodeID);

    // JSON payload
    char payload[128];
    sprintf(payload,
            "{\"node\":%d,\"temp\":%.2f,\"humidity\":%.2f,\"light\":%d}",
            data.nodeID, data.temp, data.humidity, data.light);

    // publishs it
    bool ok = client.publish(topic, payload);

    //for checking if the publishing was successful
    if(ok){
      Serial.println("MQTT published SUCCESS");
    }
    else{
      Serial.println("MQTT publish FAIL");
    }
    Serial.print("MQTT Published --- ");
    Serial.print(topic);
    Serial.print(" : ");
    Serial.println(payload);
    Serial.println("\n\n\n");
  }

}

void setupWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n WiFi Connected!");
  Serial.print("ESP32 IP address: ");
  Serial.println(WiFi.localIP());
}

void setupMQTT() {
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
  
  Serial.print("Connecting to MQTT...");

  while (!client.connected()) {
    Serial.print(".");

    if (client.connect("ESP32_Gateway")) {
      Serial.println("\n MQTT Connected!");

      client.subscribe("farm/control");
      Serial.println("Subscribed to farm/control");
    } 
    else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying...");
      delay(2000);
    }
  }
}

void sendCommandToNode(int targetNode, int command) {
  ControlCommand cmd;
  cmd.targetNode = targetNode;
  cmd.command = command;


  Serial.println("\n SENDING COMMAND ");
  Serial.print("Target Node: ");
  Serial.println(targetNode);
  Serial.print("Command: ");
  Serial.println(command == 1 ? "ON" : "OFF");
  Serial.print("Address: 0x");
  Serial.println(addresses[targetNode - 1], HEX);

  // must stop listening to transmit
  radio.stopListening();
  delay(5);

  // opens the writing pipe of the target node
  radio.openWritingPipe(addresses[targetNode - 1]);

  bool ok = radio.write(&cmd, sizeof(cmd));

  if (ok) {
    Serial.println(" Command transmission successful");
  } else {
    Serial.println(" Command transmission failed");
  }

  // Return to listening mode
  radio.startListening();
  Serial.println("\n\n\n");
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.println("\n MQTT MESSAGE RECEIVED ");
  Serial.print("Topic: ");
  Serial.println(topic);

  // converts the payload to a string
  String jsonStr;
  for (int i = 0; i < length; i++){
    jsonStr += (char)payload[i];
  }

  Serial.print("Payload: ");
  Serial.println(jsonStr);

  // Create JSON document
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, jsonStr);

  if (error) {
    Serial.print(" JSON parse error: ");
    Serial.println(error.c_str());
    Serial.println("\n\n\n");
    return;
  }

  // Extract values
  int node = doc["node"];
  int cmd  = doc["command"];

  Serial.print("Parsed --- Node: ");
  Serial.print(node);
  Serial.print(", Command: ");
  Serial.println(cmd == 1 ? "ON" : "OFF");

  // validation of nodes
  if (node < 1 || node > 4) {
    Serial.println(" Invalid node ID!");
    Serial.println("\n\n\n");
    return;
  }

  if (cmd != 0 && cmd != 1) {
    Serial.println(" Invalid command!");
    Serial.println("\n\n\n");
    return;
  }

  Serial.println("\n\n");

  // sends command
  sendCommandToNode(node, cmd);
}