#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
// HC-SRO4 Sensor
#define TRIG_PIN 17
#define ECHO_PIN 16
#define LED_ONE 13
#define LED_TWO 12
#define LED_THREE 14
// Wifi
#define SSID              "TK-gacura"
#define PASSWORD          "gisaniel924"
// Supabase
#define supabaseUrl       "https://jursmglsfqaqrxvirtiw.supabase.co"
#define supabaseKey       "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Imp1cnNtZ2xzZnFhcXJ4dmlydGl3Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NDQ3ODkxOTEsImV4cCI6MjA2MDM2NTE5MX0.ajGbf9fLrYAA0KXzYhGFCTju-d4h-iTYTeU5WfITj3k"
#define tableName         "resident_number"

// LED hold time configuration
#define LED_HOLD_TIME 5000  // LED stays on for 5 seconds (5000ms)
unsigned long lastLedChangeTime = 0;
int currentLedState = 0;  // 0: none, 1: LED_ONE, 2: LED_TWO, 3: LED_THREE

void setup() {
  Serial.begin(115200);
  WiFi.begin(SSID, PASSWORD);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi\n");
  getNumbers();
  // HC-SRO4 Sensor
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_ONE, OUTPUT);
  pinMode(LED_TWO, OUTPUT);
  pinMode(LED_THREE, OUTPUT);
  
  // Initialize all LEDs to OFF
  digitalWrite(LED_ONE, LOW);
  digitalWrite(LED_TWO, LOW);
  digitalWrite(LED_THREE, LOW);
}

void loop() {
  // Get distance from sensor - This happens immediately with every loop
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH);
  float distance = duration / 58.773;
  
  Serial.print("Raw duration: ");
  Serial.print(duration);
  Serial.print(" μs");
  Serial.print(" Distance: ");
  Serial.print(distance);
  Serial.println(" cm");
  
  // Determine target LED state based on current distance
  int targetLedState;
  if (distance <= 10) {
    targetLedState = 3;  // LED_THREE
  } 
  else if (distance <= 20) {
    targetLedState = 2;  // LED_TWO
  } 
  else if (distance <= 30) {
    targetLedState = 1;  // LED_ONE
  } 
  else {
    targetLedState = 0;  // All LEDs off
  }
  
  // Check if the hold time has passed since the last LED change
  unsigned long currentTime = millis();
  if ((currentTime - lastLedChangeTime >= LED_HOLD_TIME) && (targetLedState != currentLedState)) {
    // Update the LEDs
    updateLEDs(targetLedState);
    
    // Record the time of this LED change
    lastLedChangeTime = currentTime;
    currentLedState = targetLedState;
    
    Serial.print("LED state changed to: ");
    Serial.println(currentLedState);
  }
  else if (targetLedState != currentLedState) {
    // LED wants to change but hold time hasn't expired
    unsigned long remainingTime = LED_HOLD_TIME - (currentTime - lastLedChangeTime);
    // Only print every second to avoid flooding the serial monitor
    if (remainingTime % 1000 < 10) {
      Serial.print("LED hold active. Time remaining: ");
      Serial.print(remainingTime / 1000);
      Serial.println(" seconds");
    }
  }
}

// Function to update LEDs based on the state
void updateLEDs(int state) {
  // Turn all LEDs off first
  digitalWrite(LED_ONE, LOW);
  digitalWrite(LED_TWO, LOW);
  digitalWrite(LED_THREE, LOW);
  
  // Then turn on the appropriate LED based on state
  switch (state) {
    case 1:
      digitalWrite(LED_ONE, HIGH);
      break;
    case 2:
      digitalWrite(LED_TWO, HIGH);
      break;
    case 3:
      digitalWrite(LED_THREE, HIGH);
      break;
    // case 0 or default: all LEDs remain off
  }
}

void getNumbers()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi not connected. Attempting to reconnect...");
    while (!WiFi.reconnect())
    {
      Serial.println("Reconnecting to WiFi...");
      delay(500);
    }
    Serial.println("WiFi reconnected.");
  }
  HTTPClient http;
  String endpoint = String(supabaseUrl) + "/rest/v1/" + tableName + "?id=eq.1";
  http.begin(endpoint);
  http.addHeader("apikey", supabaseKey);
  http.addHeader("Authorization", "Bearer " + String(supabaseKey));
  int httpResponseCode = http.GET();
  if (httpResponseCode > 0)
  {
    Serial.println("\nReceiving:");
    String response = http.getString();
    Serial.println("HTTP Response code: " + String(httpResponseCode));
    Serial.println("Response: " + response);
  }
  else
  {
    Serial.println("Error in HTTP request");
    Serial.println("HTTP Response code: " + String(httpResponseCode));
    String response = http.getString();
    Serial.println("Response: " + response);
  }
  http.end();
  Serial.println("\n============================================\n");
}
