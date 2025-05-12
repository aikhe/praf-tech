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
  lcd.print("Water Monitor");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(1000);

  WiFi.begin(SSID, PASSWORD);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi\n");
  getNumbers();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    latitude = fallback_latitude;
    longitude = fallback_longitude;

    if (getWeather()) {
      getAISuggestion();
    }

    // Get location from ipinfo.io
    // if (getLocationFromIpInfo()) {
    //   Serial.println("Successfully obtained location from ipinfo.io");
    //   if (getWeather()) {
    //     getAISuggestion();
    //   }
    //   latitude = fallback_latitude;
    //   longitude = fallback_longitude;
    // } else {
    //   latitude = fallback_latitude;
    //   longitude = fallback_longitude;
    //   Serial.println("Using fallback coordinates");
    // }
  } else {
    Serial.println("\nFailed to connect to WiFi");
  }

  // Set microSD Card CS as OUTPUT and set HIGH
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  // Initialize SPI bus for microSD Card
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  // Initialize microSD card with custom SPI
  if (!SD.begin(SD_CS, SPI)) {
    Serial.println("Error accessing microSD card!");
    while (true)
      ;
  }

  Serial.println("microSD card initialized.");

  // Setup I2S
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);

  // Set Volume (0 to 21)
  audio.setVolume(100);

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

  // Bttn
  pinMode(BTTN_AI, INPUT_PULLUP);
  pinMode(BTTN_SMS, INPUT_PULLUP);

  // AI LED feedback
  pinMode(AI_LED_ONE, OUTPUT);
  pinMode(AI_LED_TWO, OUTPUT);
  pinMode(AI_LED_THREE, OUTPUT);
}

void loop() {
  unsigned long currentTime = millis();

  // Get distance from sensor - This happens immediately with every loop
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  float distance = duration / 58.773;
  
  // Update the current distance global variable for use in SMS messages
  currentDistance = distance;

  // Serial.print("Raw duration: ");
  // Serial.print(duration);
  // Serial.print(" μs");
  // Serial.print(" Distance: ");
  // Serial.print(distance);
  // Serial.println(" cm");

  // Determine target LED state based on current distance
  int targetLedState;
  if (distance <= 10) {
    targetLedState = 3;  // LED_THREE
  } else if (distance <= 20) {
    targetLedState = 2;  // LED_TWO
  } else if (distance <= 40) {
    targetLedState = 1;  // LED_ONE
  } else {
    targetLedState = 0;  // All LEDs off
  }

  // Check if any active LED has completed its 20-second duration
  if (ledActivated && currentTime - ledActivationTime >= LED_ON_DURATION) {
    // The active period is complete, turn off all LEDs
    turnOffAllLEDs();
    ledActivated = false;
    currentLedState = 0;
    Serial.println("LED 20-second active period ended, all LEDs turned off");
  }

  // If no LED is currently activated or hold time has elapsed, check if we need to change LED
  if (!ledActivated && currentTime - lastLedChangeTime >= LED_HOLD_TIME && targetLedState != 0) {
    // Activate the new LED
    updateLEDs(targetLedState);
    currentLedState = targetLedState;
    ledActivated = true;
    ledActivationTime = currentTime;
    lastLedChangeTime = currentTime;

    Serial.print("New LED activated: LED ");
    Serial.print(currentLedState);
    Serial.println(" will stay on for 20 seconds");
  }
  // If an LED is active but user moved to a new distance zone and hold time passed
  else if (ledActivated && targetLedState != 0 && targetLedState != currentLedState && currentTime - lastLedChangeTime >= LED_HOLD_TIME) {
    // Change to new LED within the active period
    updateLEDs(targetLedState);
    currentLedState = targetLedState;
    lastLedChangeTime = currentTime;
    // Note: We don't reset the ledActivationTime here, so the 20-second countdown continues

    Serial.print("LED changed during active period to: LED ");
    Serial.println(currentLedState);
  }
  // If we're in the active period, occasionally show time remaining
  else if (ledActivated && currentTime % 1000 < 10) {
    unsigned long remainingTime = LED_ON_DURATION - (currentTime - ledActivationTime);
    Serial.print("LED active time remaining: ");
    Serial.print(remainingTime / 1000);
    Serial.println(" seconds");
  }

  int reading = digitalRead(BTTN_AI);

  // toggle leds (pressed or released)
  if (reading != lastButtonState) {
    previousMillis = millis();  // Reset debounce timer (only need once to toggle running state and set the value by currentMillis)
  }

  if ((millis() - previousMillis) > 50) {  // Debounce time (50ms) for consistent toggling, always run once button was clicked
    if (reading != buttonState) {
      buttonState = reading;
      Serial.print("buttonState: ");
      Serial.println(buttonState);

      // Only toggle leds state if the button is pressed (0/LOW)
      if (buttonState == LOW) {
        isRunning = !isRunning;
        Serial.print("isRunning: ");
        Serial.println(isRunning);
      }
    }
  }

  lastButtonState = reading;  // Reset button state to 1/HIGH

  if (digitalRead(BTTN_AI) == LOW) {
    Serial.println("AI button pressed!");

    // First, get a fresh AI suggestion before playing any sounds
    Serial.println("Getting fresh AI weather suggestion...");
    bool suggestionUpdated = getAISuggestion();
    if (suggestionUpdated) {
      Serial.println("Successfully updated AI suggestion!");
    } else {
      Serial.println("Couldn't update AI suggestion, using existing one");
    }

    // Start playing notification sound immediately
    audio.connecttoFS(SD, "AI-NOTIF.mp3");

    // Define sequence
    int leds[] = { AI_LED_ONE, AI_LED_TWO, AI_LED_THREE };
    int sequence[] = { 0, 1, 2, 0, 1, 2, -1 };  // -1 = all LEDs on
    int currentStep = 0;
    unsigned long previousStepTime = millis();
    const long stepInterval = 200;

    // Run LED sequence while audio is playing
    while (audio.isRunning() || currentStep < 7) {
      if (audio.isRunning()) {
        audio.loop();
      }

      unsigned long currentTime = millis();
      if (currentTime - previousStepTime >= stepInterval) {
        previousStepTime = currentTime;

        // Turn off all LEDs before applying the next step
        if (currentStep < 7) {
          for (int j = 0; j < 3; j++) {
            digitalWrite(leds[j], LOW);
          }
        }

        if (currentStep < 7) {
          if (sequence[currentStep] == -1) {
            // Turn all LEDs ON
            delay(200);
            for (int j = 0; j < 3; j++) {
              digitalWrite(leds[j], HIGH);
            }
          } else {
            digitalWrite(leds[sequence[currentStep]], HIGH);
          }
          currentStep++;
        }
      }
    }

    Serial.print("isRunning: ");
    Serial.println(isRunning);
    
    // Format a message to display on serial console for visibility
    String aiStatusMessage = String("🤖 AI WEATHER & FLOOD UPDATE 🤖\n\n") +
                          "📍 Location: " + location + "\n" +
                          "🌤️ Current Weather: " + weatherDescription + "\n" +
                          "🌡️ Temperature: " + String(temperature, 1) + "°C (feels like " + String(feelsLike, 1) + "°C)\n" +
                          "💧 Humidity: " + String(humidity, 0) + "%\n\n" +
                          "🚨 Flood Status: ";
                          
    // Add flood status information
    if (currentLedState == 1) {
      aiStatusMessage += "LOW RISK - Initial flooding detected\n\n";
    } else if (currentLedState == 2) {
      aiStatusMessage += "MEDIUM RISK - Significant flooding detected\n\n";
    } else if (currentLedState == 3) {
      aiStatusMessage += "HIGH RISK - Severe flooding detected\n\n";
    } else {
      aiStatusMessage += "No flooding detected at this time\n\n";
    }
    
    // Add AI suggestion
    aiStatusMessage += "🤖 AI Recommendation:\n" + AISuggestion;
    
    // Print the formatted message to serial
    Serial.println("\n===============================");
    Serial.println(aiStatusMessage);
    Serial.println("===============================\n");

    // LEDs REMAIN ON here...
    playFloodWarning();  // Play audio version of AI suggestion
    
    // NOW turn off all LEDs after AI suggestion is done
    digitalWrite(AI_LED_ONE, LOW);
    digitalWrite(AI_LED_TWO, LOW);
    digitalWrite(AI_LED_THREE, LOW);

    lastPlayTime = millis();
    delay(1000);  // debounce delay
  }

  if (digitalRead(BTTN_SMS) == LOW) {
    Serial.println("SMS button pressed!");

    // Start playing notification sound immediately
    audio.connecttoFS(SD, "SMS-SENT.mp3");

    // Define sequence
    int leds[] = { AI_LED_ONE, AI_LED_TWO, AI_LED_THREE };
    int sequence[] = { 0, 1, 2, 0, 1, 2, -1 };  // -1 = all LEDs on
    int currentStep = 0;
    unsigned long previousStepTime = millis();
    const long stepInterval = 200;

    // Run LED sequence while audio is playing
    while (audio.isRunning() || currentStep < 7) {
      if (audio.isRunning()) {
        audio.loop();
      }

      unsigned long currentTime = millis();
      if (currentTime - previousStepTime >= stepInterval) {
        previousStepTime = currentTime;

        // Turn off all LEDs before applying the next step
        if (currentStep < 7) {
          for (int j = 0; j < 3; j++) {
            digitalWrite(leds[j], LOW);
          }
        }

        if (currentStep < 7) {
          if (sequence[currentStep] == -1) {
            // Turn all LEDs ON
            delay(200);
            for (int j = 0; j < 3; j++) {
              digitalWrite(leds[j], HIGH);
            }
          } else {
            digitalWrite(leds[sequence[currentStep]], HIGH);
          }
          currentStep++;
        }
      }
    }

    // Create alert message based on current alert state
    String alertMessage = "";
    float distance = currentDistance; // Get current water level reading

    if (currentLedState == 1) {
      alertMessage = String("⚠️ FLOOD ALERT - LEVEL 1 ⚠️\n\n") +
                    "📍 Location: " + location + "\n" +
                    "🔍 Current water level: " + String(distance, 1) + " cm\n" +
                    "⚠️ Status: ALERT - Initial flooding detected\n\n" +
                    "🚨 PRECAUTIONS:\n" +
                    "- Monitor water levels\n" +
                    "- Prepare emergency supplies\n" +
                    "- Stay tuned for updates\n\n" +
                    "From: PRAF Technology Flood Monitoring System";
    } else if (currentLedState == 2) {
      alertMessage = String("🔴 FLOOD CRITICAL - LEVEL 2 🔴\n\n") +
                    "📍 Location: " + location + "\n" +
                    "🔍 Current water level: " + String(distance, 1) + " cm\n" +
                    "⚠️ Status: CRITICAL - Significant flooding\n\n" +
                    "🚨 URGENT ACTIONS REQUIRED:\n" +
                    "- Move valuables to higher ground\n" +
                    "- Prepare for possible evacuation\n" +
                    "- Avoid flooded areas\n" +
                    "- Charge communication devices\n\n" +
                    "From: PRAF Technology Flood Monitoring System";
    } else if (currentLedState == 3) {
      alertMessage = String("🚨 FLOOD WARNING - LEVEL 3 🚨\n\n") +
                    "📍 Location: " + location + "\n" +
                    "🔍 Current water level: " + String(distance, 1) + " cm\n" +
                    "⚠️ Status: WARNING - Severe flooding\n\n" +
                    "🚨 EMERGENCY ACTIONS REQUIRED:\n" +
                    "- EVACUATE immediately if instructed\n" +
                    "- Move to higher ground NOW\n" +
                    "- Follow emergency routes\n" +
                    "- Do NOT attempt to cross floodwaters\n" +
                    "- Call emergency services if trapped\n\n" +
                    "From: PRAF Technology Flood Monitoring System";
    } else {
      // Default message when no flood level is active
      alertMessage = String("🚨 FLOOD ALERT! 🚨\n\n") +
                    "📱 System Check:\n\n" +
                    "📍 Location: " + location + "\n" +
                    "🌤️ Weather: " + weatherDescription + "\n" +
                    "🌡️ Temperature: " + String(temperature, 1) + "°C\n" +
                    "🌡️ Feels like: " + String(feelsLike, 1) + "°C\n" +
                    "💧 Humidity: " + String(humidity, 0) + "%\n\n" +
                    "🤖 AI Weather Update:\n" +
                    AISuggestion + "\n\n" +
                    "From: PRAF Technology";
    }

    // Ensure WiFi is connected before sending SMS
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected. Attempting to reconnect...");
      reconnectWiFi();
    }

    // Send SMS to all registered numbers
    if (registeredPhoneNumbers.size() > 0) {
      Serial.println("📲 Sending SMS to " + String(registeredPhoneNumbers.size()) + " registered numbers");

      for (String toNumber : registeredPhoneNumbers) {
        sendHttpSMS(fromSmsNumber, toNumber.c_str(), alertMessage.c_str());
        Serial.println("✅ SMS sent to: " + toNumber);
        delay(300);  // Small delay between sending messages
      }
    } else {
      // Use the default number if no registered numbers
      sendHttpSMS(fromSmsNumber, toSmsNumber, alertMessage.c_str());
      Serial.println("✅ SMS sent to default number: " + String(toSmsNumber));
    }

    // Play confirmation sound
    audio.connecttoFS(SD, "SMS-SENT.mp3");
    while (audio.isRunning()) {
      audio.loop();
    }

    delay(1200);  // debounce delay

    digitalWrite(AI_LED_ONE, LOW);
    digitalWrite(AI_LED_TWO, LOW);
    digitalWrite(AI_LED_THREE, LOW);
  }

  // Check if first file is done playing and we need to play the alert
  if (playingFirstFile && !audio.isRunning()) {
    playingFirstFile = false;

    // Play the corresponding alert file
    if (currentAlertState == 1) {
      audio.connecttoFS(SD, "LOW-ALERT.mp3");
    } else if (currentAlertState == 2) {
      audio.connecttoFS(SD, "MEDIUM-ALERT.mp3");
    } else if (currentAlertState == 3) {
      audio.connecttoFS(SD, "HIGH-ALERT.mp3");
    }
  }

  // Inside your loop() function, add this:
  if (currentTime - lastCheckTime >= checkInterval && !audio.isRunning()) {
    lastCheckTime = currentTime;
    getNumbers();  // Check for new database entries
    Serial.println("Registered phone numbers:");
    for (const String& number : registeredPhoneNumbers) {
      Serial.println(number);
    }
  }

  audio.loop();
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

// 🔊 Speak in smart chunks
void speakTextInChunks(String text, int maxLength) {
  // Use a smaller chunk size
  int chunkSize = 60;  // Reduced from 100

  int start = 0;
  while (start < text.length()) {
    int end = start + chunkSize;

    // Ensure we don't split in the middle of a word
    if (end < text.length()) {
      // Prefer ending at punctuation
      int punctEnd = end;
      while (punctEnd > start && text[punctEnd] != '.' && text[punctEnd] != ',' && text[punctEnd] != ';' && text[punctEnd] != ':') {
        punctEnd--;
      }

      // If we found punctuation, use that as the end point
      if (punctEnd > start && (text[punctEnd] == ',' || text[punctEnd] == ';' || text[punctEnd] == ':')) {
        end = punctEnd + 1;  // Include the punctuation
      } else {
        // Otherwise find a space
        while (end > start && text[end] != ' ') {
          end--;
        }
        if (end == start) {
          end = start + chunkSize;  // Worst case, just cut at max length
        }
      }
    }

    String chunk = text.substring(start, end);
    chunk.trim();  // Remove any leading/trailing spaces

    if (chunk.length() > 0) {
      Serial.println("Playing chunk: '" + chunk + "'");
      Serial.println("Start: " + String(start) + ", End: " + String(end));

      audio.connecttospeech(chunk.c_str(), TTS_GOOGLE_LANGUAGE);
      while (audio.isRunning()) {
        audio.loop();
      }
    }

    start = end;
  }
}

void playFloodWarning() {
  speakTextInChunks(AISuggestion, 100);
}

// Function to update LEDs based on the state
void updateLEDs(int state) {
  // Turn all LEDs off first
  turnOffAllLEDs();

  // Then turn on the appropriate LED based on state
  switch (state) {
    case 1:
      digitalWrite(LED_ONE, HIGH);
      currentAlertState = 1;
      playingFirstFile = true;
      audio.connecttoFS(SD, "LOW-FLOOD-HIGH.mp3");
      break;
    case 2:
      digitalWrite(LED_TWO, HIGH);
      currentAlertState = 2;
      playingFirstFile = true;
      audio.connecttoFS(SD, "MEDIUM-FLOOD-HIGH2.mp3");
      break;
    case 3:
      digitalWrite(LED_THREE, HIGH);
      currentAlertState = 3;
      playingFirstFile = true;
      audio.connecttoFS(SD, "HIGH-FLOOD-HIGH2.mp3");
      break;
      // case 0 or default: all LEDs remain off
  }
}

void handleAudioSequence() {
  static int pendingAlertState = 0;
  static bool playingFlood = false;
  static unsigned long floodStartTime = 0;

  // If we're not currently playing anything, but we have a pending alert
  if (!audio.isRunning() && playingFlood) {
    // First audio finished playing, now play the alert
    playingFlood = false;

    switch (pendingAlertState) {
      case 1:
        audio.connecttoFS(SD, "LOW-ALERT.mp3");
        break;
      case 2:
        audio.connecttoFS(SD, "MEDIUM-ALERT.mp3");
        break;
      case 3:
        audio.connecttoFS(SD, "HIGH-ALERT.mp3");
        break;
    }

    // Reset pending state
    pendingAlertState = 0;
  }
}

// Function to turn off all LEDs
void turnOffAllLEDs() {
  digitalWrite(LED_ONE, LOW);
  digitalWrite(LED_TWO, LOW);
  digitalWrite(LED_THREE, LOW);
}

bool getLocationFromIpInfo() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  HTTPClient http;
  bool success = false;

  http.begin("https://ipinfo.io/json");
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("IPInfo Response: " + payload);

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error && doc.containsKey("loc")) {
      String loc = doc["loc"].as<String>();
      int commaIndex = loc.indexOf(',');

      if (commaIndex > 0) {
        latitude = loc.substring(0, commaIndex).toFloat();
        longitude = loc.substring(commaIndex + 1).toFloat();

        String city = "Unknown";
        String region = "Unknown";
        String country = "Unknown";

        if (doc.containsKey("city")) {
          city = doc["city"].as<String>();
        }

        if (doc.containsKey("region")) {
          region = doc["region"].as<String>();
        }

        if (doc.containsKey("country")) {
          country = doc["country"].as<String>();
        }

        Serial.print("Detected Location: ");
        Serial.print(city);
        Serial.print(", ");
        Serial.print(region);
        Serial.print(", ");
        Serial.println(country);

        Serial.print("Coordinates from ipinfo.io: ");
        Serial.print(latitude, 6);
        Serial.print(", ");
        Serial.println(longitude, 6);

        success = true;
      }
    }
  } else {
    Serial.print("Failed to get location from IPInfo, HTTP code: ");
    Serial.println(httpCode);
  }
  http.end();

  return success;
}

bool getWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  HTTPClient http;
  bool success = false;

  String url = String("http://api.openweathermap.org/data/2.5/weather?q=Caloocan,PH&appid=") + weatherApiKey + "&units=metric&lang=en";

  Serial.println("Weather API URL: " + url);
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("OpenWeather Response: " + payload);

    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      String cityName = doc["name"].as<String>();
      location = cityName;

      String country = "unknown";
      if (doc.containsKey("sys") && doc["sys"].containsKey("country")) {
        country = doc["sys"]["country"].as<String>();
      }

      weatherDescription = "unknown";
      if (doc.containsKey("weather") && doc["weather"][0].containsKey("description")) {
        weatherDescription = doc["weather"][0]["description"].as<String>();
      }

      temperature = 0;
      feelsLike = 0;
      humidity = 0;
      if (doc.containsKey("main")) {
        temperature = doc["main"]["temp"].as<float>();
        feelsLike = doc["main"]["feels_like"].as<float>();
        humidity = doc["main"]["humidity"].as<float>();
      }

      Serial.println("==== WEATHER INFORMATION ====");
      Serial.print("Location: ");
      Serial.print(cityName);
      Serial.print(", ");
      Serial.println(country);
      Serial.print("Weather: ");
      Serial.println(weatherDescription);
      Serial.print("Temperature: ");
      Serial.print(temperature);
      Serial.println("°C");
      Serial.print("Feels like: ");
      Serial.print(feelsLike);
      Serial.println("°C");
      Serial.print("Humidity: ");
      Serial.print(humidity);
      Serial.println("%");
      Serial.println("============================");

      success = true;
    } else {
      Serial.println("Error parsing weather data");
    }
  } else {
    Serial.println("Failed to connect to OpenWeather API, HTTP code: " + String(httpCode));
  }
  http.end();

  return success;
}

bool getAISuggestion() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi not connected. Cannot get AI suggestion.");
    return false;
  }

  HTTPClient http;
  bool success = false;
  
  Serial.println("🤖 Requesting AI weather suggestion...");

  // Make sure we have reasonable weather data
  if (temperature == 0.0 || weatherDescription.length() == 0) {
    if (getWeather()) {
      Serial.println("✅ Weather data refreshed successfully");
    } else {
      Serial.println("⚠️ Could not refresh weather data, using default values");
    }
  }

  String prompt = "Provide a short and helpful suggestion to inform residents about the current weather and keep them safe.\n\n";
  prompt += "- Weather Details:\n";
  prompt += "  - City: " + location + "\n";
  prompt += "  - Weather: " + weatherDescription + "\n";
  prompt += "  - Temperature: " + String(temperature, 2) + "°C\n";
  prompt += "  - Feels like: " + String(feelsLike, 2) + "°C\n";
  prompt += "  - Humidity: " + String(humidity, 2) + "%\n\n";
  
  // Add flood status information to the prompt
  prompt += "- Current Water Level Status:\n";
  if (currentLedState == 1) {
    prompt += "  - LOW FLOOD RISK detected\n";
  } else if (currentLedState == 2) {
    prompt += "  - MEDIUM FLOOD RISK detected\n";
  } else if (currentLedState == 3) {
    prompt += "  - HIGH FLOOD RISK detected - EMERGENCY SITUATION\n";
  } else {
    prompt += "  - No flooding detected at this time\n";
  }
  prompt += "\n";

  prompt += "Instructions:\n";
  prompt += "- Write the message like a weather forecast-casual, clear, and understandable for most people.\n";
  prompt += "- Start with: \"PRAF Technology Weather Update:\".\n";
  prompt += "- Next sentence should note the location/city:\".\n";
  prompt += "- The message should be one sentence long and include a note that it's from PRAF Technology.\n";
  prompt += "- If the weather poses a flood risk, alert the residents.\n";
  prompt += "- If flooding is unlikely, suggest a safe way to deal with the weather while reassuring them.\n";
  prompt += "- Maintain a formal tone and avoid AI-like phrasing.\n";
  prompt += "- Do not use uncertain words like \"naman.\"\n";
  prompt += "- And most importantly mainly use tagalog.\n";
  prompt += "- Structure:\n";
  prompt += "  1. Start with the flood update.\n";
  prompt += "  2. Then, provide the weather update.\n";
  prompt += "  3. End with a safety tip.\n";
  prompt += "- Do not include greetings-just start with the message.";

  StaticJsonDocument<2048> requestDoc;
  JsonArray contents = requestDoc.createNestedArray("contents");
  JsonObject content = contents.createNestedObject();
  JsonArray parts = content.createNestedArray("parts");
  JsonObject part = parts.createNestedObject();
  part["text"] = prompt;

  JsonObject generationConfig = requestDoc.createNestedObject("generationConfig");
  generationConfig["temperature"] = 0.7;
  generationConfig["topP"] = 0.9;
  generationConfig["maxOutputTokens"] = 200;

  String requestBody;
  serializeJson(requestDoc, requestBody);

  String geminiUrl = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key=" + String(geminiApiKey);

  // Set a timeout value for the HTTP request
  http.setTimeout(10000); // 10 second timeout

  http.begin(geminiUrl);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(requestBody);

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("🔄 Gemini API Response received");

    StaticJsonDocument<2048> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, payload);

    if (!error && responseDoc.containsKey("candidates") && 
        responseDoc["candidates"][0].containsKey("content") && 
        responseDoc["candidates"][0]["content"].containsKey("parts") && 
        responseDoc["candidates"][0]["content"]["parts"][0].containsKey("text")) {

      String aiMessage = responseDoc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
      
      // Verify we got a non-empty response before updating
      if (aiMessage.length() > 0) {
        AISuggestion = aiMessage;
        Serial.println("\n==== 🤖 AI WEATHER SUGGESTION 🤖 ====");
        Serial.println(aiMessage);
        Serial.println("======================================\n");
        success = true;
      } else {
        Serial.println("❌ Received empty response from Gemini API");
      }
    } else {
      Serial.println("❌ Error parsing Gemini API response");
      if (error) {
        Serial.print("DeserializationError: ");
        Serial.println(error.c_str());
      }
    }
  } else {
    Serial.print("❌ Failed to connect to Gemini API, HTTP code: ");
    Serial.println(httpCode);
    Serial.println("Request Body: " + requestBody);
  }

  http.end();
  
  // If API call failed but we have an existing AI suggestion, we can still return true
  // to indicate we can continue with what we have
  if (!success && AISuggestion.length() > 0) {
    Serial.println("⚠️ Using existing AI suggestion as fallback");
    return true;
  }
  
  return success;
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
          digitalWrite(AI_LED_ONE, HIGH);
          digitalWrite(AI_LED_TWO, HIGH);
          digitalWrite(AI_LED_THREE, HIGH);

          knownIds.push_back(id);
          Serial.print("New Number Added: ");
          Serial.println(number);

          delay(1500);

          digitalWrite(AI_LED_ONE, LOW);
          digitalWrite(AI_LED_TWO, LOW);
          digitalWrite(AI_LED_THREE, LOW);
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
