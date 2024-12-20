#include <SoftwareSerial.h>
#include <ArduinoJson.h>

// Initialise Arduino to NodeMCU (5=Rx & 6=Tx)
SoftwareSerial nodemcu(5, 6);

// Turbidity value variable
int sensorValue;

void setup() {
  // Initialize serial communication
  Serial.begin(9600);
 
  // Initialize software serial for NodeMCU communication
  nodemcu.begin(9600);
 
  delay(1000);
  Serial.println("Turbidity Program started");
}

void loop() {
  // // Create JSON document (use StaticJsonDocument for stack allocation)
  StaticJsonDocument<200> doc;
 
  // // Read Sensor
  read_sensor();
 
  // // Assign collected data to JSON Object
  doc["turbidity"] = sensorValue;
 
  // // Send data to NodeMCU
  serializeJson(doc, nodemcu);
  nodemcu.println(); // Add a newline to help with parsing
 
  // // Optionally, serialize to Serial for debugging
  serializeJsonPretty(doc, Serial);
  Serial.println();
 
  // // Delay between readings
  delay(1000);
}

void read_sensor() {
  Serial.print("Turbidity Sensor Value: ");
  sensorValue = analogRead(A0);
  Serial.println(sensorValue);
}