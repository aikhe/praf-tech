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
#define BTTN_SMS 22

// Supabase
#define supabaseUrl "https://jursmglsfqaqrxvirtiw.supabase.co"
#define supabaseKey "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Imp1cnNtZ2xzZnFhcXJ4dmlydGl3Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NDQ3ODkxOTEsImV4cCI6MjA2MDM2NTE5MX0.ajGbf9fLrYAA0KXzYhGFCTju-d4h-iTYTeU5WfITj3k"
#define tableName "resident_number"

// AI suggestions using Gemini AI
const char* weatherApiKey = "7970309436bc52d518c7e71e314b8053";
const char* geminiApiKey = "AIzaSyD_g_WAsPqPKxltdOJt8VZw4uu359D3XXA";

// HttpSMS API credentials
const char* httpSmsApiKey = "iFqOahA-gXvOzLHlt3mHWIs5kLsqQ11FFu8QblKwxKMzDj49mLyw_dpEgMkIDFsS";

// Create Audio object
Audio audio;

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
#define LED_ON_DURATION 600000  // Time LED stays on once activated (10 minutes)
#define ALERT_COOLDOWN 600000 // 10 minutes cooldown between same level alerts (in milliseconds)

unsigned long lastLedChangeTime = 0;  // For tracking LED hold time (debounce)
unsigned long ledActivationTime = 0;  // For tracking how long LED has been active
unsigned long lastAlertTime = 0;      // For tracking when the last alert was sent
int currentLedState = 0;              // 0: none, 1: LED_ONE, 2: LED_TWO, 3: LED_THREE
int lastAlertLevel = 0;               // Tracks the level of the last alert
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
#define AI_LED_THREE 21

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

// Add these global variables at the top with other globals
bool isSendingSMS = false;
unsigned long lastSMSTime = 0;
int currentSMSIndex = 0;
String currentSMSMessage = "";

const char* fromSmsNumber = "+639649687066";

LiquidCrystal_I2C lcd(0x27, 16, 2); // Try 0x3F if 0x27 doesn't work

void setup() {
  Serial.begin(115200);
  WiFi.begin(SSID, PASSWORD);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi\n");

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Starting");
  lcd.setCursor(0, 1);
  lcd.print("Device....");

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

  // Play confirmation sound
  audio.connecttoFS(SD, "DEVICE-START-VOICe.mp3");
  while (audio.isRunning()) {
    audio.loop();
  }
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

  // Serial.print("Raw duration: ");
  // Serial.print(duration);
  // Serial.print(" μs");
  // Serial.print(" Distance: ");
  // Serial.print(distance);
  // Serial.println(" cm");

  // Determine target LED state based on current distance
  int targetLedState;
  if (distance <= 12) {
    targetLedState = 3;  // LED_THREE

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Water Dis: ");
    lcd.print((int)distance);  // Print the actual distance value
    lcd.print("cm");     // Optional: add unit
    lcd.setCursor(0, 1);
    lcd.print("Status: Warning");
    Serial.println(distance);
  } else if (distance <= 22) {
    targetLedState = 2;  // LED_TWO

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Water Dis: ");
    lcd.print((int)distance);  // Print the actual distance value
    lcd.print("cm");     // Optional: add unit
    lcd.setCursor(0, 1);
    lcd.print("Status: Warning");
    Serial.println(distance);
  } else if (distance <= 40) {
    targetLedState = 1;  // LED_ONE

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Water Dis: ");
    lcd.print((int)distance);  // Print the actual distance value
    lcd.print("cm");     // Optional: add unit
    lcd.setCursor(0, 1);
    lcd.print("Status: Warning");
    Serial.println(distance);
  } else {
    targetLedState = 0;  // All LEDs off
  }

  lcd.setCursor(0, 0);
  lcd.print(location);
  lcd.print(", ");
  lcd.print((int)temperature);
  lcd.print("'C");
  lcd.setCursor(0, 1);
  lcd.print(weatherDescription);

  // Check if enough time has passed since last LED change (debounce)
  bool enoughTimePassedSinceLastChange = (currentTime - lastLedChangeTime) >= LED_HOLD_TIME;
  
  // Check if any active LED has completed its duration
  if (ledActivated && currentTime - ledActivationTime >= LED_ON_DURATION) {
    turnOffAllLEDs();
    ledActivated = false;
    currentLedState = 0;
    Serial.println("LED 60-second active period ended, all LEDs turned off");
  }

  // Only process new alerts if enough time has passed since last change
  bool canTriggerAlert = enoughTimePassedSinceLastChange && 
                        (!ledActivated || 
                         targetLedState != lastAlertLevel ||
                         (targetLedState == lastAlertLevel && (currentTime - lastAlertTime) >= ALERT_COOLDOWN));

  if (canTriggerAlert && targetLedState != 0) {
    if (targetLedState == lastAlertLevel) {
      Serial.print("Same level detected after cooldown: Level ");
      Serial.println(targetLedState);
    }
    
    // Activate the new LED
    updateLEDs(targetLedState);
    currentLedState = targetLedState;
    ledActivated = true;
    ledActivationTime = currentTime;
    lastLedChangeTime = currentTime;

    Serial.print("New LED activated: LED ");
    Serial.print(currentLedState);
    Serial.println(" will stay on for 60 seconds");
  } else if (!enoughTimePassedSinceLastChange && targetLedState != 0) {
    // Log remaining hold time
    unsigned long remainingHoldTime = (LED_HOLD_TIME - (currentTime - lastLedChangeTime)) / 1000;
    Serial.print("Hold time active. ");
    Serial.print(remainingHoldTime);
    Serial.println(" seconds remaining before next detection.");
  } else if (targetLedState == lastAlertLevel && targetLedState != 0) {
    // Log remaining cooldown time
    unsigned long remainingCooldown = (ALERT_COOLDOWN - (currentTime - lastAlertTime)) / 1000;
    Serial.print("Alert cooldown active for level ");
    Serial.print(targetLedState);
    Serial.print(". ");
    Serial.print(remainingCooldown);
    Serial.println(" seconds remaining.");
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
    Serial.print("AISuggestion: ");
    Serial.println(AISuggestion);

    // LEDs REMAIN ON here...

    playFloodWarning();  // Optional sound or alert before AI suggestion

    // NOW turn off all LEDs after AI suggestion is done
    digitalWrite(AI_LED_ONE, LOW);
    digitalWrite(AI_LED_TWO, LOW);
    digitalWrite(AI_LED_THREE, LOW);

    getAISuggestion();  // This might take time — LEDs stay on through it

    lastPlayTime = millis();
    delay(1000);  // debounce delay
  }

  if (digitalRead(BTTN_SMS) == LOW) {
    Serial.println("SMS button pressed!");

    int leds[] = { AI_LED_ONE, AI_LED_TWO, AI_LED_THREE };
    int sequence[] = { 0, 1, 2, 0, 1, 2, -1 };  // -1 indicates "turn all LEDs on"

    for (int i = 0; i <= 6; i++) {
      // Turn off all LEDs
      for (int j = 0; j < 3; j++) {
        digitalWrite(leds[j], LOW);
      }

      if (sequence[i] == -1) {
        // Turn all LEDs on
        delay(100);
        for (int j = 0; j < 3; j++) {
          digitalWrite(leds[j], HIGH);
        }
      } else {
        // Turn on the current LED
        digitalWrite(leds[sequence[i]], HIGH);
      }

      delay(200);
    }

    // Play confirmation sound
    audio.connecttoFS(SD, "SMS-SENT-VOICE.mp3");
    while (audio.isRunning()) {
      audio.loop();
    }

    // Create weather update message
    String weatherMessage = "📱 PRAF WEATHER UPDATE 📱\n\n";
    weatherMessage += "📍 Location: " + location + "\n";
    weatherMessage += "🌤️ Weather: " + weatherDescription + "\n";
    weatherMessage += "🌡️ Temperature: " + String(temperature, 1) + "°C\n";
    weatherMessage += "🌡️ Feels like: " + String(feelsLike, 1) + "°C\n";
    weatherMessage += "💧 Humidity: " + String(humidity, 0) + "%\n\n";
    weatherMessage += "🤖 AI Weather Update:\n" + AISuggestion + "\n\n";
    weatherMessage += "From: PRAF Technology";

    // Send SMS to all registered numbers
    if (registeredPhoneNumbers.size() > 0) {
      Serial.println("Sending weather update SMS to " + String(registeredPhoneNumbers.size()) + " registered numbers");

      for (String toNumber : registeredPhoneNumbers) {
        sendHttpSMS(fromSmsNumber, toNumber.c_str(), weatherMessage.c_str());
        Serial.println("Weather update SMS sent to: " + toNumber);
      }
    } else {
      Serial.println("No registered numbers to send weather update to");
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
      // Create flood alert message first (before starting audio)
      String floodAlertMessage = "🚨 FLOOD ALERT: LOW RISK 💧\n\n";
      floodAlertMessage += "📍 Location: " + location + "\n";
      floodAlertMessage += "TIPS: Monitor water levels. Keep valuables elevated. Avoid flood-prone areas.\n\n";
      floodAlertMessage += "From: PRAF Technology";
      
      // Start audio playback first
      audio.connecttoFS(SD, "LOW-ALERT-HIGH.mp3");
      while (audio.isRunning()) {
        audio.loop();
      }

      for (String toNumber : registeredPhoneNumbers) {
        sendHttpSMS(fromSmsNumber, toNumber.c_str(), floodAlertMessage.c_str());
        Serial.println("Flood Alert SMS sent to: " + toNumber);
      }
      
      // Queue SMS to be sent in background
      sendFloodAlert(floodAlertMessage);
      Serial.println(floodAlertMessage);
      
    } else if (currentAlertState == 2) {
      // Create medium flood alert message
      String floodAlertMessage = "🚨 FLOOD ALERT: MEDIUM RISK ⚠️\n\n";
      floodAlertMessage += "📍 Location: " + location + "\n";
      floodAlertMessage += "TIPS: Move to higher ground. Prepare evacuation supplies. Stay informed.\n\n";
      floodAlertMessage += "From: PRAF Technology";
      
      audio.connecttoFS(SD, "MEDIUM-ALERT-HIGH.mp3");
      while (audio.isRunning()) {
        audio.loop();
      }
      
      for (String toNumber : registeredPhoneNumbers) {
        sendHttpSMS(fromSmsNumber, toNumber.c_str(), floodAlertMessage.c_str());
        Serial.println("Medium Flood Alert SMS sent to: " + toNumber);
      }
      
      sendFloodAlert(floodAlertMessage);
      Serial.println(floodAlertMessage);
      
    } else if (currentAlertState == 3) {
      // Create high flood alert message
      String floodAlertMessage = "🚨 FLOOD ALERT: HIGH RISK ⛔\n\n";
      floodAlertMessage += "📍 Location: " + location + "\n";
      floodAlertMessage += "TIPS: Evacuate immediately to designated centers. Follow authorities' instructions.\n\n";
      floodAlertMessage += "From: PRAF Technology";
      
      audio.connecttoFS(SD, "HIGH-ALERT-HIGH.mp3");
      while (audio.isRunning()) {
        audio.loop();
      }
      
      for (String toNumber : registeredPhoneNumbers) {
        sendHttpSMS(fromSmsNumber, toNumber.c_str(), floodAlertMessage.c_str());
        Serial.println("High Flood Alert SMS sent to: " + toNumber);
      }
      
      sendFloodAlert(floodAlertMessage);
      Serial.println(floodAlertMessage);
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
  Serial.println("Preparing to send SMS...");

  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect("api.httpsms.com", 443)) {
    Serial.println("Connection to HttpSMS API failed");
    return;
  }

  // Create JSON payload
  DynamicJsonDocument doc(1024);
  doc["content"] = body;
  doc["from"] = from;
  doc["to"] = to;

  String jsonPayload;
  serializeJson(doc, jsonPayload);

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

  // Read and print the response
  Serial.println("Reading SMS API response:");
  // while (client.connected() || client.available()) {
  //   if (client.available()) {
  //     String line = client.readStringUntil('\n');
  //     Serial.println(line);
  //   }
  // }

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
  speakTextInChunks(AISuggestion, 100);  // Split into chunks of ~100 characters
}

// Function to update LEDs based on the state
void updateLEDs(int state) {
  unsigned long currentTime = millis();
  
  // Check if this is the same alert level and if enough time has passed
  if (state == lastAlertLevel && (currentTime - lastAlertTime) < ALERT_COOLDOWN) {
    // Not enough time has passed since the last alert of this level
    Serial.println("Alert cooldown active. Skipping repeat alert.");
    return;
  }

  // Turn all LEDs off first
  turnOffAllLEDs();

  // Then turn on the appropriate LED based on state and send SMS
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
      audio.connecttoFS(SD, "MEDIUM-FLOOD-HIGH.mp3");
      break;

    case 3:
      digitalWrite(LED_THREE, HIGH);
      currentAlertState = 3;
      playingFirstFile = true;
      audio.connecttoFS(SD, "HIGH-FLOOD-HIGH.mp3");
      break;
      // case 0 or default: all LEDs remain off
  }

  // Update the last alert time and level
  if (state != 0) {
    lastAlertTime = currentTime;
    lastAlertLevel = state;
    Serial.print("New alert set at level ");
    Serial.print(state);
    Serial.println(". Cooldown started.");
  }
}

// Modify the sendFloodAlert function to be non-blocking
void sendFloodAlert(String alertMessage) {
  if (registeredPhoneNumbers.size() > 0) {
    Serial.println("Sending flood alert SMS to " + String(registeredPhoneNumbers.size()) + " registered numbers");
    isSendingSMS = true;
    currentSMSIndex = 0;
    currentSMSMessage = alertMessage;
    lastSMSTime = 0; // Reset the timer
  } else {
    Serial.println("No registered numbers to send flood alert to");
  }
}

// Add this function to handle non-blocking SMS sending
void handleSMS() {
  if (!isSendingSMS) return;
  
  unsigned long currentTime = millis();
  
  // Check if it's time to send the next SMS
  if (currentTime - lastSMSTime >= 300) { // 300ms delay between messages
    if (currentSMSIndex < registeredPhoneNumbers.size()) {
      String toNumber = registeredPhoneNumbers[currentSMSIndex];
      sendHttpSMS(fromSmsNumber, toNumber.c_str(), currentSMSMessage.c_str());
      Serial.println("Flood alert SMS sent to: " + toNumber);
      currentSMSIndex++;
      lastSMSTime = currentTime;
    } else {
      // All messages sent
      isSendingSMS = false;
      currentSMSIndex = 0;
      currentSMSMessage = "";
    }
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

void getAISuggestion() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  HTTPClient http;

  String prompt = "Provide a short and helpful suggestion to inform residents about the current weather and keep them safe.\n\n";
  prompt += "- Weather Details:\n";
  prompt += "  - City: " + location + "\n";
  prompt += "  - Weather: " + weatherDescription + "\n";
  prompt += "  - Temperature: " + String(temperature, 2) + "°C\n";
  prompt += "  - Feels like: " + String(feelsLike, 2) + "°C\n";
  prompt += "  - Humidity: " + String(humidity, 2) + "%\n\n";
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

  http.begin(geminiUrl);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(requestBody);

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("Gemini API Response: " + payload);

    StaticJsonDocument<2048> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, payload);

    if (!error && responseDoc.containsKey("candidates") && responseDoc["candidates"][0].containsKey("content") && responseDoc["candidates"][0]["content"].containsKey("parts") && responseDoc["candidates"][0]["content"]["parts"][0].containsKey("text")) {

      String aiMessage = responseDoc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
      AISuggestion = aiMessage;

      Serial.println("\n==== AI WEATHER SUGGESTION ====");
      Serial.println(aiMessage);
      Serial.println("===============================\n");
    } else {
      Serial.println("Error parsing Gemini API response");
    }
  } else {
    Serial.print("Failed to connect to Gemini API, HTTP code: ");
    Serial.println(httpCode);
    Serial.println("Request Body: " + requestBody);
  }

  http.end();
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

          // Play confirmation sound
          audio.connecttoFS(SD, "NEW-NUM-REG-HIGH.mp3");
          while (audio.isRunning()) {
            audio.loop();
          }

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

void reconnectWiFi() {
  Serial.println("WiFi not connected. Attempting to reconnect...");
  while (!WiFi.reconnect()) {
    Serial.println("Reconnecting to WiFi...");
    delay(500);
  }
  Serial.println("WiFi reconnected.");
}
