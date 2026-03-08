// Poultry Heater Control - ML + Web Dashboard
// Complete system with ML predictions and real-time web monitoring

#include <DHT.h>
#include <WiFi.h>
#include <WebServer.h>
#include "poultry_heater_model.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"


const char* ssid = "Nana's A36";
const char* password = "mq7thgkt6dfs7az";


// PIN DEFINITIONS
#define DHT_PIN 4
#define DHT_TYPE DHT22
#define LDR_PIN 34
#define LED_PIN 2
#define BUZZER_PIN 5

// ============================================
// OBJECTS
// ============================================
DHT dht(DHT_PIN, DHT_TYPE);
WebServer server(80);

// ML Globals
constexpr int kTensorArenaSize = 8 * 1024;
uint8_t tensor_arena[kTensorArenaSize];
tflite::MicroInterpreter* interpreter;
TfLiteTensor* input;
TfLiteTensor* output;

// State tracking
bool previousHeaterState = false;
unsigned long lastReadTime = 0;
const unsigned long READ_INTERVAL = 3000;

// Current readings (for web display)
float currentTemp = 0;
float currentHumidity = 0;
float currentLight = 0;
float currentProb = 0;
bool currentHeaterState = false;
bool isUncertain = false;

// Function prototypes
float readLDR();
void beep(int times, int duration);

// ============================================
// SETUP
// ============================================
void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("   POULTRY HEATER CONTROL SYSTEM");
    Serial.println("   ML + Web Dashboard Version");
    Serial.println("....................\n");

    // Initialize hardware
    Serial.println("Initializing hardware...");
    dht.begin();
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println("✓ DHT22 sensor initialized");
    Serial.println("✓ LED/Relay pin configured");
    Serial.println("✓ Buzzer configured");

    // Connect to WiFi
    Serial.println("\nConnecting to WiFi...");
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✓ WiFi connected!");
        Serial.print("✓ IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.println("\n📱 Open your browser and go to:");
        Serial.print("   http://");
        Serial.println(WiFi.localIP());
        beep(3, 100);  // Success beeps
    } else {
        Serial.println("\n⚠ WiFi connection failed");
        Serial.println("Continuing without web interface...");
        beep(5, 100);  // Error beeps
    }

    // Setup web server routes
    server.on("/", []() {
        String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Poultry Heater Control</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
        }
        .header {
            text-align: center;
            color: white;
            margin-bottom: 30px;
        }
        .header h1 {
            font-size: 2.5em;
            margin-bottom: 10px;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.3);
        }
        .header p {
            font-size: 1.2em;
            opacity: 0.9;
        }
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
            gap: 20px;
            margin-bottom: 20px;
        }
        .card {
            background: white;
            border-radius: 15px;
            padding: 25px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
            transition: transform 0.3s ease;
        }
        .card:hover {
            transform: translateY(-5px);
        }
        .card-title {
            font-size: 0.9em;
            color: #666;
            text-transform: uppercase;
            letter-spacing: 1px;
            margin-bottom: 10px;
        }
        .card-value {
            font-size: 3em;
            font-weight: bold;
            color: #667eea;
            margin-bottom: 5px;
        }
        .card-unit {
            font-size: 1.2em;
            color: #999;
        }
        .status-card {
            grid-column: span 2;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
        }
        .status-card .card-title {
            color: rgba(255,255,255,0.8);
        }
        .status-indicator {
            display: flex;
            align-items: center;
            gap: 15px;
            margin: 20px 0;
        }
        .status-light {
            width: 60px;
            height: 60px;
            border-radius: 50%;
            box-shadow: 0 0 20px currentColor;
            animation: pulse 2s infinite;
        }
        .status-light.on {
            background: #4ade80;
            color: #4ade80;
        }
        .status-light.off {
            background: #6b7280;
            color: #6b7280;
        }
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }
        .status-text {
            font-size: 2em;
            font-weight: bold;
        }
        .warning {
            background: #fbbf24;
            color: #78350f;
            padding: 15px;
            border-radius: 10px;
            margin-top: 15px;
            display: none;
        }
        .warning.show {
            display: block;
        }
        .progress-bar {
            width: 100%;
            height: 30px;
            background: #e5e7eb;
            border-radius: 15px;
            overflow: hidden;
            margin: 15px 0;
        }
        .progress-fill {
            height: 100%;
            background: linear-gradient(90deg, #667eea, #764ba2);
            transition: width 0.5s ease;
            display: flex;
            align-items: center;
            justify-content: center;
            color: white;
            font-weight: bold;
        }
        .timestamp {
            text-align: center;
            color: white;
            margin-top: 20px;
            opacity: 0.8;
        }
        @media (max-width: 768px) {
            .status-card {
                grid-column: span 1;
            }
            .header h1 {
                font-size: 1.8em;
            }
            .card-value {
                font-size: 2.5em;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🐔 Poultry Heater Control</h1>
            <p>Real-time ML-Powered Monitoring</p>
        </div>

        <div class="grid">
            <div class="card">
                <div class="card-title">Temperature</div>
                <div class="card-value" id="temp">--</div>
                <div class="card-unit">°C</div>
            </div>

            <div class="card">
                <div class="card-title">Humidity</div>
                <div class="card-value" id="humidity">--</div>
                <div class="card-unit">%</div>
            </div>

            <div class="card">
                <div class="card-title">Light Level</div>
                <div class="card-value" id="light">--</div>
                <div class="card-unit">/100</div>
            </div>

            <div class="card">
                <div class="card-title">ML Confidence</div>
                <div class="card-value" id="prob">--</div>
                <div class="card-unit">%</div>
                <div class="progress-bar">
                    <div class="progress-fill" id="probBar">0%</div>
                </div>
            </div>

            <div class="card status-card">
                <div class="card-title">Heater Status</div>
                <div class="status-indicator">
                    <div class="status-light off" id="statusLight"></div>
                    <div class="status-text" id="statusText">OFF</div>
                </div>
                <div class="warning" id="warning">
                    ⚠️ Uncertain prediction - Human verification advised
                </div>
            </div>
        </div>

        <div class="timestamp" id="timestamp">Last update: --</div>
    </div>

    <script>
        function updateData() {
            fetch('/data')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('temp').textContent = data.temperature.toFixed(1);
                    document.getElementById('humidity').textContent = data.humidity.toFixed(1);
                    document.getElementById('light').textContent = Math.round(data.light);
                    
                    const probPercent = (data.probability * 100).toFixed(1);
                    document.getElementById('prob').textContent = probPercent;
                    document.getElementById('probBar').style.width = probPercent + '%';
                    document.getElementById('probBar').textContent = probPercent + '%';
                    
                    const statusLight = document.getElementById('statusLight');
                    const statusText = document.getElementById('statusText');
                    const warning = document.getElementById('warning');
                    
                    if (data.heaterOn) {
                        statusLight.className = 'status-light on';
                        statusText.textContent = 'ON';
                    } else {
                        statusLight.className = 'status-light off';
                        statusText.textContent = 'OFF';
                    }
                    
                    if (data.uncertain) {
                        warning.classList.add('show');
                    } else {
                        warning.classList.remove('show');
                    }
                    
                    const now = new Date();
                    document.getElementById('timestamp').textContent = 
                        'Last update: ' + now.toLocaleTimeString();
                })
                .catch(error => console.error('Error:', error));
        }

        updateData();
        setInterval(updateData, 2000);
    </script>
</body>
</html>
)rawliteral";
        server.send(200, "text/html", html);
    });

    server.on("/data", []() {
        String json = "{";
        json += "\"temperature\":" + String(currentTemp) + ",";
        json += "\"humidity\":" + String(currentHumidity) + ",";
        json += "\"light\":" + String(currentLight) + ",";
        json += "\"probability\":" + String(currentProb) + ",";
        json += "\"heaterOn\":" + String(currentHeaterState ? "true" : "false") + ",";
        json += "\"uncertain\":" + String(isUncertain ? "true" : "false");
        json += "}";
        server.send(200, "application/json", json);
    });

    server.begin();
    Serial.println("✓ Web server started");

    // Load ML model
    Serial.println("\nLoading TensorFlow Lite model...");
    const tflite::Model* model = tflite::GetModel(poultry_heater_model);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.println(" Model schema mismatch!");
        while (1) {
            beep(3, 100);
            delay(1000);
        }
    }

    static tflite::MicroMutableOpResolver<6> resolver;
    resolver.AddFullyConnected();
    resolver.AddRelu();
    resolver.AddLogistic();
    resolver.AddSoftmax();
    resolver.AddQuantize();
    resolver.AddDequantize();

    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize
    );

    interpreter = &static_interpreter;

    if (interpreter->AllocateTensors() != kTfLiteOk) {
        Serial.println("Tensor allocation failed!");
        while (1) {
            beep(3, 100);
            delay(1000);
        }
    }

    input = interpreter->input(0);
    output = interpreter->output(0);
    Serial.println("✓ ML model loaded successfully");

    Serial.println("\nWaiting for sensors to stabilize...");
    delay(2000);

   
    Serial.println("   SYSTEM READY - MONITORING ACTIVE");
    

    beep(2, 100);  // System ready
}

// MAIN LOOP
void loop() {
    // Handle web requests
    server.handleClient();

    // Timing control - read every 3 seconds
    if (millis() - lastReadTime < READ_INTERVAL) return;
    lastReadTime = millis();

    // Read sensors
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    float light = readLDR();

    // Validate readings
    if (isnan(temperature) || isnan(humidity)) {
        Serial.println("⚠ Sensor read error - retrying...");
        beep(1, 100);
        return;
    }

    // Update global variables for web display
    currentTemp = temperature;
    currentHumidity = humidity;
    currentLight = light;

    // Scale inputs
    input->data.f[0] = (temperature - SCALER_MEAN[0]) / SCALER_STD[0];
    input->data.f[1] = (humidity    - SCALER_MEAN[1]) / SCALER_STD[1];
    input->data.f[2] = (light       - SCALER_MEAN[2]) / SCALER_STD[2];

    // Run inference
    if (interpreter->Invoke() != kTfLiteOk) {
        Serial.println("Model inference failed!");
        beep(1, 100);
        return;
    }

    float prob = output->data.f[0];
    currentProb = prob;

    // Confidence logic
    bool heaterOn;
    isUncertain = false;

    if (prob >= 0.7) {
        heaterOn = true;
    } else if (prob <= 0.3) {
        heaterOn = false;
    } else {
        isUncertain = true;
        heaterOn = previousHeaterState;
    }

    currentHeaterState = heaterOn;

    // State change detection & beep
    if (heaterOn != previousHeaterState) {
        beep(1, 200);
        previousHeaterState = heaterOn;
        Serial.println("\n>>> HEATER STATE CHANGED <<<");
    }

    // Uncertainty alert
    if (isUncertain) {
        beep(2, 80);
        Serial.println("⚠ UNCERTAIN PREDICTION - HUMAN CHECK ADVISED");
    }

    // Control heater
    digitalWrite(LED_PIN, heaterOn ? HIGH : LOW);

    // Display results
    Serial.print("[");
    Serial.print(millis() / 1000);
    Serial.print("s] ");
    Serial.print("Temp: ");
    Serial.print(temperature, 1);
    Serial.print("°C | Humidity: ");
    Serial.print(humidity, 1);
    Serial.print("% | Light: ");
    Serial.print(light, 0);
    Serial.print(" | Prob: ");
    Serial.print(prob, 3);
    Serial.print(" | Heater: ");
    Serial.println(heaterOn ? "ON ✓" : "OFF");
}


// HELPER FUNCTIONS

float readLDR() {
    long sum = 0;
    const int samples = 10;
    
    for (int i = 0; i < samples; i++) {
        sum += analogRead(LDR_PIN);
        delay(5);
    }
    
    int avgValue = sum / samples;
    float ldrValue = map(avgValue, 0, 4095, 0, 100);
    ldrValue = constrain(ldrValue, 0, 100);
    
    return ldrValue;
}

void beep(int times, int duration) {
    for (int i = 0; i < times; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(duration);
        digitalWrite(BUZZER_PIN, LOW);
        if (i < times - 1) delay(duration);
    }
}