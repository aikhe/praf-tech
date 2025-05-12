#include "Arduino.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>  
#include "Audio.h"
#include "SD.h"
#include "FS.h"
#include "SPI.h"
#include <vector>
#include <LiquidCrystal_I2C.h>

// Wifi
#define SSID "TK-gacura"
#define PASSWORD "gisaniel924"

// microSD Card Reader connections
#define SD_CS 5
#define SPI_MOSI 23
#define SPI_MISO 19
#define SPI_SCK 18

// I2S Connections (MAX98357)
#define I2S_DOUT 25
#define I2S_BCLK 27
#define I2S_LRC 26

// HC-SRO4 Sensor
#define TRIG_PIN 17
#define ECHO_PIN 16

#define LED_ONE 13
#define LED_TWO 12
#define LED_THREE 14

// Button
#define BTTN_AI 4
#define BTTN_SMS 2

// Supabase
#define supabaseUrl "https://jursmglsfqaqrxvirtiw.supabase.co"
#define supabaseKey "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Imp1cnNtZ2xzZnFhcXJ4dmlydGl3Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NDQ3ODkxOTEsImV4cCI6MjA2MDM2NTE5MX0.ajGbf9fLrYAA0KXzYhGFCTju-d4h-iTYTeU5WfITj3k"
#define tableName "resident_number"

// AI suggestions using Gemini AI
const char* weatherApiKey = "7970309436bc52d518c7e71e314b8053";
const char* geminiApiKey = "AIzaSyD_g_WAsPqPKxltdOJt8VZw4uu359D3XXA";

// HttpSMS API credentials
const char* httpSmsApiKey = "MNJmgF7kRvUrTfj4fqDUbrzwoVFpMToWdTbiUx3sQ6jreYnbnu7bym-rQG3kB8_U";
const char* fromSmsNumber = "+639649687066";
const char* toSmsNumber = "+639649687066";

// Create Audio object
Audio audio;

// LCD Setup - Assuming standard 16x2 I2C LCD at address 0x27
// Adjust address if your LCD uses a different one
LiquidCrystal_I2C lcd(0x27, 16, 2);

float fallback_latitude = 14.6571;
float fallback_longitude = 120.9841;

float latitude = 0.0;
float longitude = 0.0;

String location = "";
String weatherDescription = "";
float temperature = 0.0;
float feelsLike = 0.0;
float humidity = 0.0;

String AISuggestion = "";

#define TTS_GOOGLE_LANGUAGE "tl"  // "tl" for Tagalog

// Dual timer configuration
#define LED_HOLD_TIME 6000     // Minimum time (ms) before LED can change (debounce)
#define LED_ON_DURATION 60000  // Time LED stays on once activated (20 seconds)

unsigned long lastLedChangeTime = 0;  // For tracking LED hold time (debounce)
unsigned long ledActivationTime = 0;  // For tracking how long LED has been active
int currentLedState = 0;              // 0: none, 1: LED_ONE, 2: LED_TWO, 3: LED_THREE
bool ledActivated = false;            // Whether any LED is currently in its 20-second active period

// Sentence chunks
#define MAX_CHUNKS 10
String sentenceChunks[MAX_CHUNKS];
uint8_t numChunks = 0;

unsigned long lastPlayTime = 0;
const unsigned long playInterval = 440000UL;

// Global variable to track audio state
bool playingFirstFile = false;
int currentAlertState = 0;

#define AI_LED_ONE 32
#define AI_LED_TWO 15
#define AI_LED_THREE 33

int buttonState = 0;
int lastButtonState = HIGH;

bool isRunning = false;

unsigned long previousMillis = 0;
unsigned long interval = 300;
int currentLED = 0;

// Variables to track database state
std::vector<int> knownIds;
unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 10000;  // Check every 10 seconds

std::vector<String> registeredPhoneNumbers;

// Current water distance reading
float currentDistance = 0.0;

void setup() {
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SMS Tester");
  lcd.setCursor(0, 1);
  lcd.print("Press BTN to test");
  
  WiFi.begin(SSID, PASSWORD);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi\n");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Initialize the button pin
  pinMode(BTTN_SMS, INPUT_PULLUP);
  
  // LEDs for visual feedback
  pinMode(LED_ONE, OUTPUT);
  pinMode(LED_TWO, OUTPUT);
  pinMode(LED_THREE, OUTPUT);
  
  // Initialize all LEDs to OFF
  digitalWrite(LED_ONE, LOW);
  digitalWrite(LED_TWO, LOW);
  digitalWrite(LED_THREE, LOW);
  
  // Get initial list of phone numbers
  getNumbers();
}

void loop() {
  // Check if button is pressed
  if (digitalRead(BTTN_SMS) == LOW) {
    Serial.println("📱 SMS button pressed!");
    
    // Visual feedback - turn on LEDs
    digitalWrite(LED_ONE, HIGH);
    digitalWrite(LED_TWO, HIGH);
    digitalWrite(LED_THREE, HIGH);
    
    // Display on LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sending SMS...");
    
    // Create default alert message with emojis and formatting
    String alertMessage = String("🚨 FLOOD ALERT! 🚨\n\n") +
                     "📱 System Check:\n\n" +
                     "📍 Location: " + location + "\n" +
                     "🌤️ Weather: " + weatherDescription + "\n" +
                     "🌡️ Temperature: " + String(temperature, 1) + "°C\n" +
                     "🌡️ Feels like: " + String(feelsLike, 1) + "°C\n" +
                     "💧 Humidity: " + String(humidity, 0) + "%\n\n" +
                     "🤖 AI Weather Update:\n" +
                     AISuggestion + "\n\n" +
                     "From: PRAF Technology";

    // Ensure WiFi is connected before sending SMS
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected. Attempting to reconnect...");
      reconnectWiFi();
    }

    // Send SMS to all registered numbers
    if (registeredPhoneNumbers.size() > 0) {
      Serial.println("📲 Sending SMS to " + String(registeredPhoneNumbers.size()) + " registered numbers");
      lcd.setCursor(0, 1);
      lcd.print("To: " + String(registeredPhoneNumbers.size()) + " numbers");

      for (String toNumber : registeredPhoneNumbers) {
        sendHttpSMS(fromSmsNumber, toNumber.c_str(), alertMessage.c_str());
        Serial.println("✅ SMS sent to: " + toNumber);
        delay(300);  // Small delay between sending messages
      }
    } else {
      // Use the default number if no registered numbers
      lcd.setCursor(0, 1);
      lcd.print("To: Default number");
      sendHttpSMS(fromSmsNumber, toSmsNumber, alertMessage.c_str());
      Serial.println("✅ SMS sent to default number: " + String(toSmsNumber));
    }

    // Visual feedback - done
    delay(1000); 
    digitalWrite(LED_ONE, LOW);
    digitalWrite(LED_TWO, LOW);
    digitalWrite(LED_THREE, LOW);
    
    // Update LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SMS Sent!");
    lcd.setCursor(0, 1);
    lcd.print("Press again to test");
    
    delay(2000);  // debounce delay
  }

  // Periodically check for new phone numbers
  unsigned long currentTime = millis();
  if (currentTime - lastCheckTime >= checkInterval) {
    lastCheckTime = currentTime;
    getNumbers();  // Check for new database entries
    Serial.println("Registered phone numbers:");
    for (const String& number : registeredPhoneNumbers) {
      Serial.println(number);
    }
  }
}

void sendHttpSMS(const char* from, const char* to, const char* body) {
  Serial.print("📱 Sending SMS to: ");
  Serial.println(to);
  
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10); // Set a lower timeout to prevent blocking too long
  
  Serial.println("🔄 Connecting to SMS API...");
  int retry = 0;
  while (!client.connect("api.httpsms.com", 443) && retry < 3) {
    Serial.println("Connection to HttpSMS API failed, retrying...");
    delay(100); // Short delay before retry
    retry++;
  }
  
  if (retry >= 3) {
    Serial.println("Connection to HttpSMS API failed after multiple attempts");
    return;
  }
  
  // Create JSON payload
  DynamicJsonDocument doc(1024);
  doc["content"] = body;
  doc["from"] = from;
  doc["to"] = to;
  String jsonPayload;
  serializeJson(doc, jsonPayload);
  doc.clear(); // Free memory immediately
  
  // Send POST request
  client.println("POST /v1/messages/send HTTP/1.1");
  client.println("Host: api.httpsms.com");
  client.print("x-api-key: ");
  client.println(httpSmsApiKey);
  client.println("Content-Type: application/json");
  client.print("Content-Length: ");
  client.println(jsonPayload.length());
  client.println("Connection: close");
  client.println();
  client.println(jsonPayload);
  
  Serial.println("SMS Request sent!");
  
  // Read and print the response with proper timing
  Serial.println("Reading SMS API response:");
  unsigned long timeout = millis();
  while ((client.connected() || client.available()) && millis() - timeout < 5000) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      Serial.println(line);
    } else {
      // Short delay while waiting for data
      delay(10);
    }
  }
  
  client.stop();
  Serial.println("SMS Connection closed");
}

void getNumbers() {
  if (WiFi.status() != WL_CONNECTED) {
    reconnectWiFi();
  }

  // Clear the existing phone numbers array
  registeredPhoneNumbers.clear();

  HTTPClient http;
  String endpoint = String(supabaseUrl) + "/rest/v1/" + tableName;

  http.begin(endpoint);
  http.addHeader("apikey", supabaseKey);
  http.addHeader("Authorization", "Bearer " + String(supabaseKey));
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.GET();

  if (httpResponseCode == 200) {
    String response = http.getString();

    // Parse JSON response
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
      Serial.print("JSON deserialization failed: ");
      Serial.println(error.c_str());
    } else {
      JsonArray array = doc.as<JsonArray>();

      Serial.print("Found ");
      Serial.print(array.size());
      Serial.println(" phone numbers.");

      for (JsonVariant entry : array) {
        int id = entry["id"];
        String number = entry["number"].as<String>();

        // Add number to our array
        registeredPhoneNumbers.push_back(number);

        // Add ID to our known IDs list if not already there
        bool isNewId = true;
        for (int knownId : knownIds) {
          if (id == knownId) {
            isNewId = false;
            break;
          }
        }

        if (isNewId) {
          digitalWrite(LED_ONE, HIGH);
          digitalWrite(LED_TWO, HIGH);
          digitalWrite(LED_THREE, HIGH);

          knownIds.push_back(id);
          Serial.print("New Number Added: ");
          Serial.println(number);

          delay(1500);

          digitalWrite(LED_ONE, LOW);
          digitalWrite(LED_TWO, LOW);
          digitalWrite(LED_THREE, LOW);
        }
      }

      Serial.print("Total registered numbers: ");
      Serial.println(registeredPhoneNumbers.size());
    }
  } else {
    Serial.print("Error getting entries. HTTP Response code: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}

// CHECKPOINT #2

void reconnectWiFi() {
  Serial.println("WiFi not connected. Attempting to reconnect...");
  while (!WiFi.reconnect()) {
    Serial.println("Reconnecting to WiFi...");
    delay(500);
  }
  Serial.println("WiFi reconnected.");
}
