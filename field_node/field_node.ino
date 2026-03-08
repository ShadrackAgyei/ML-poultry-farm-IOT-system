#include <SPI.h> 
#include <nRF24L01.h> 
#include <RF24.h> 
#include <DHT.h>

RF24 radio(4, 5); // CE, CSN 

#define DHTPIN 15
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define LDR_PIN 34
const uint8_t NODE_ID = 1; // change for different nodes
#define ControlPIN 2  // onboard pin

const uint64_t addresses[] = { 
  0xA1A1A1A101LL, 0xA1A1A1A102LL, 0xA1A1A1A103LL, 
  0xA1A1A1A104LL, 0xA1A1A1A105LL
}; 
  
typedef struct { 
  int nodeID; 
  float temp; 
  float humidity; 
  int light; 
} sensorData; 

typedef struct {
  int targetNode;
  int command;
} ControlCommand;

unsigned long lastSensorSend = 0;
const unsigned long SENSOR_INTERVAL = 5000;

void setup() { 
  Serial.begin(9600); 

  pinMode(ControlPIN, OUTPUT);
  digitalWrite(ControlPIN, LOW);

  dht.begin();
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  radio.begin(); 
  radio.setAutoAck(false);
  radio.setPALevel(RF24_PA_MIN); 

  radio.openWritingPipe(addresses[NODE_ID - 1]);
  radio.openReadingPipe(1, addresses[NODE_ID - 1]);
  radio.startListening();  // Start in RX mode

  Serial.print("Field Node ");
  Serial.print(NODE_ID);
  Serial.println(" Started (Always Listening Mode)!");
} 

void loop() { 
  // Always check for commands
  checkForCommands();
  
  // Send sensor data every 5 seconds
  if (millis() - lastSensorSend >= SENSOR_INTERVAL) {
    sendSensorData();
    lastSensorSend = millis();
  }
  
  delay(10);  // Small delay
}

void sendSensorData() {
  sensorData data; 
  data.nodeID = NODE_ID; 
  
  data.humidity = dht.readHumidity();
  data.temp = dht.readTemperature();
  int ldrRaw = analogRead(LDR_PIN);
  data.light = map(ldrRaw, 0, 1795, 0, 100);
  
  if (isnan(data.humidity) || isnan(data.temp)) {
    data.temp = -999;
    data.humidity = -999;
  }
  
  Serial.println("========== SENDING SENSOR DATA ==========");
  Serial.print("Temp: ");
  Serial.print(data.temp);
  Serial.print("°C, Humidity: ");
  Serial.print(data.humidity);
  Serial.print("%, Light: ");
  Serial.println(data.light);
  
  radio.stopListening();
  delay(5);
  
  bool ok = radio.write(&data, sizeof(sensorData)); 
  Serial.println(ok ? "✓ Sent" : "✗ Failed");
  Serial.println(".............\n");
  
  radio.startListening();
}

void checkForCommands() {
  if (radio.available()) {
    ControlCommand cmd;
    radio.read(&cmd, sizeof(cmd));

    if (cmd.targetNode == NODE_ID) {
      Serial.println("\nCOMMAND FOR ME! ");
      Serial.print("Command: ");
      Serial.println(cmd.command == 1 ? "ON" : "OFF");

      digitalWrite(ControlPIN, cmd.command == 1 ? HIGH : LOW);
      
      Serial.print("LED is now: ");
      Serial.println(cmd.command == 1 ? "ON" : "OFF");
      Serial.print("GPIO state: ");
      Serial.println(digitalRead(ControlPIN));
      Serial.println("*\n");
    }
  }
}