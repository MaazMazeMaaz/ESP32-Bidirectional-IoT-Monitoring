#include <WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>
#include <ArduinoJson.h>

// --- Pin Definitions ---
#define DHT_PIN 15
#define LED_PIN 13
#define POT_PIN 34

// --- WiFi & Public MQTT Broker Settings ---
// Wokwi provides a built-in virtual WiFi access point named "Wokwi-GUEST"
const char* ssid = "Wokwi-GUEST"; 
const char* password = ""; 

// We are using a free public MQTT broker for testing
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

// Topics
const char* telemetry_topic = "factory/machine_1/telemetry";
const char* control_topic = "factory/machine_1/control";

WiFiClient espClient;
PubSubClient client(espClient);
DHTesp dht;

unsigned long lastMsgTime = 0;
const long interval = 1000; // Send telemetry every 5 seconds

// --- 1. THE CALLBACK (When Node-RED sends data TO the ESP32) ---
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);

  // Convert payload bytes to a string
  String messageTemp;
  for (int i = 0; i < length; i++) {
    messageTemp += (char)payload[i];
  }
  Serial.print("Received Command: ");
  Serial.println(messageTemp);

  // If we receive "ON", light up the LED. Otherwise, turn it off.
  if (String(topic) == control_topic) {
    if (messageTemp == "true") {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED turned ON");
    } else if (messageTemp == "false") {
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED turned OFF");
    }
  }
}

// --- Helper: Connecting to WiFi and MQTT ---
void setup_wifi() {
  delay(10);
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Generate a unique client ID based on ESP32 MAC Address
    String clientId = "ESP32Client-" + String(WiFi.macAddress());
    
    if (client.connect(clientId.c_str())) {
      Serial.println("connected!");
      // Subscribing to the control topic to receive remote commands
      client.subscribe(control_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" trying again in 5 seconds");
      delay(5000);
    }
  }
}

// --- 2. SETUP (Run Once) ---
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(POT_PIN, INPUT);
  
  dht.setup(DHT_PIN, DHTesp::DHT22);
  setup_wifi();
  
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback); // Tell the client where to send incoming packets
}

// --- 3. LOOP (Run Continuously) ---
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // Keep the MQTT background connection alive

  unsigned long now = millis();
  if (now - lastMsgTime > interval) {
    lastMsgTime = now;

    // Read Sensors
    float humidity = dht.getHumidity();
    float temperature = dht.getTemperature();
    
    // Read Potentiometer and map 0-4095 ESP32 ADC range to a 0-100% dial
    int rawPot = analogRead(POT_PIN);
    int mappedLoad = map(rawPot, 0, 4095, 0, 100); 

    // Create JSON payload
    StaticJsonDocument<200> doc;
    doc["temp"] = round(temperature * 10) / 10.0; // round to 1 decimal place
    doc["hum"] = round(humidity * 10) / 10.0;
    doc["load"] = mappedLoad;

    // Serialize JSON into a char array
    char jsonBuffer[512];
    serializeJson(doc, jsonBuffer);

    // Publish JSON string to the Broker
    Serial.print("Publishing telemetry: ");
    Serial.println(jsonBuffer);
    client.publish(telemetry_topic, jsonBuffer);
  }
}
