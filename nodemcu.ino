#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// WiFi settings
const char *ssid = "ssid";          
const char *password = "password";   

// MQTT Broker settings
const char *mqtt_broker = "broker";  // EMQX broker endpoint
const char *mqtt_turbidity_topic = "emqx/esp8266/turbidity";     // MQTT topic for Turbidity data
const char *mqtt_drain_pump_topic = "emqx/esp8266/pump";     // MQTT topic for Pump control
const char *mqtt_automatic_drain_pump_topic = "emqx/esp8266/automatic-drain-pump"; // MQTT topic for automatic mode
const char *mqtt_username = "username";      // MQTT username for authentication
const char *mqtt_password = "password";  // MQTT password for authentication
const int mqtt_port = 8883;              // MQTT port (TCP)

WiFiClientSecure espClient;
PubSubClient mqtt_client(espClient);

// DRAIN PUMP pins
#define DRAIN_PUMP 5  // GPIO 5 (D1) for DRAIN_PUMP
#define SUPPLY_PUMP 4  // GPIO 4 (D2) for SUPPLY_PUMP

// State variables
bool drain_pump_state = false;
bool supply_pump_state = false;
bool automatic_drain_pump_enabled = false;
int turbidity_threshold = 0;

void connectToWiFi() {
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to the WiFi network");
}

void connectToMQTTBroker() {
    while (!mqtt_client.connected()) {
        String client_id = "nodemcu-client-" + String(WiFi.macAddress());
        Serial.printf("Connecting to MQTT Broker as %s.....\n", client_id.c_str());
        
        if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
            Serial.println("Connected to MQTT broker");
            
            // Subscribe to Drain Pump control topics
            mqtt_client.subscribe(mqtt_drain_pump_topic);
            mqtt_client.subscribe(mqtt_automatic_drain_pump_topic);
            
            Serial.println("Subscribed to Drain Pump control topics");
        } else {
            Serial.print("Failed to connect to MQTT broker, rc=");
            Serial.print(mqtt_client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message received on topic: ");
    Serial.println(topic);
    
    String message;
    for (int i = 0; i < length; i++) {
        message += (char) payload[i];
    }
    
    // Handle Automatic Drain Pump Control
    if (String(topic) == mqtt_automatic_drain_pump_topic) {
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, message);
        
        if (!error) {
            String mode = doc["mode"].as<String>();
            if (mode == "on") {
                automatic_drain_pump_enabled = true;
                turbidity_threshold = doc["threshold"].as<int>();
                drain_pump_state = false;
                digitalWrite(DRAIN_PUMP, HIGH);
                Serial.printf("Automatic Drain Pump Enabled. Threshold: %d\n", turbidity_threshold);
            } else if (mode == "off") {
                automatic_drain_pump_enabled = false;
                turbidity_threshold = 0;
                drain_pump_state = false;
                digitalWrite(DRAIN_PUMP, HIGH);
                Serial.println("Automatic Drain Pump Disabled");
            }
        }
        return;
    }
    
    // DRAIN_PUMP Control
    if (message == "on" || message == "off") {
        // Manually Disable Automatic Mode only through this specific control channel
        automatic_drain_pump_enabled = false;
        turbidity_threshold = 0;
        Serial.println("Automatic mode disabled due to manual control");

        if (message == "on" && !drain_pump_state) {
            digitalWrite(DRAIN_PUMP, LOW);
            drain_pump_state = true;
            Serial.println("DRAIN_PUMP is turned on");
        }
        if (message == "off" && drain_pump_state) {
            digitalWrite(DRAIN_PUMP, HIGH);
            drain_pump_state = false;
            Serial.println("DRAIN_PUMP is turned off");
        }
    }

    // SUPPLY_PUMP Control
    if (message == "on_supply" || message == "off_supply") {
        Serial.println("Automatic mode disabled due to manual control");

        if (message == "on_supply" && !supply_pump_state) {
            digitalWrite(SUPPLY_PUMP, LOW);
            supply_pump_state = true;
            Serial.println("SUPPLY_PUMP is turned on");
        }
        if (message == "off_supply" && supply_pump_state) {
            digitalWrite(SUPPLY_PUMP, HIGH);
            supply_pump_state = false;
            Serial.println("SUPPLY_PUMP is turned off");
        }
    }
}

void setup() {
    Serial.begin(9600);
    
    // Initialize PUMP pins
    pinMode(DRAIN_PUMP, OUTPUT);
    pinMode(SUPPLY_PUMP, OUTPUT);
    // write HIGH to turn off pump, because of Normally Open Relay
    digitalWrite(DRAIN_PUMP, HIGH);
    digitalWrite(SUPPLY_PUMP, HIGH);

    // Connect to WiFi
    connectToWiFi();
    
    // Setup MQTT
    espClient.setInsecure();
    mqtt_client.setServer(mqtt_broker, mqtt_port);
    mqtt_client.setCallback(mqttCallback);
    
    // Connect to MQTT Broker
    connectToMQTTBroker();
}

void loop() {
    if (!mqtt_client.connected()) {
        connectToMQTTBroker();
    }
    
    mqtt_client.loop();
    
    if (Serial.available()) {
      // Create a JSON document
      StaticJsonDocument<200> doc;

      // Deserialize the JSON data
      DeserializationError error = deserializeJson(doc, Serial);

      // Test if parsing succeeds
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
      }

      // Extract the Turbidity value
      int turbidityValue = doc["turbidity"];
      
      // Publish Turbidity value to MQTT
      char turbidityMessage[50];
      sprintf(turbidityMessage, "%d", turbidityValue);
      mqtt_client.publish(mqtt_turbidity_topic, turbidityMessage);

      // Print the Turbidity value
      Serial.print("Received Turbidity Value: ");
      Serial.println(turbidityValue);

      // Automatic Drain Pump Logic
      if (automatic_drain_pump_enabled && turbidityValue <= turbidity_threshold) {
        if (!drain_pump_state) {
          digitalWrite(DRAIN_PUMP, LOW);
          drain_pump_state = true;
          Serial.println("DRAIN_PUMP automatically turned on due to low turbidity");
        }
      } else if (automatic_drain_pump_enabled && turbidityValue > turbidity_threshold) {
        digitalWrite(DRAIN_PUMP, HIGH);
        drain_pump_state = false;
        Serial.println("DRAIN_PUMP automatically turned off due to high turbidity");
      }

    }
    delay(500);
}