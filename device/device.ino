#include "Arduino.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "Audio.h"
#include "SD.h"
#include "FS.h"
#include "SPI.h"

// Wifi
#define SSID              "TK-gacura"
#define PASSWORD          "gisaniel924"

// microSD Card Reader connections
#define SD_CS          5
#define SPI_MOSI      23 
#define SPI_MISO      19
#define SPI_SCK       18

// I2S Connections (MAX98357)
#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26

// HC-SRO4 Sensor
#define TRIG_PIN 17
#define ECHO_PIN 16
#define LED_ONE 13
#define LED_TWO 12
#define LED_THREE 14

// Button
#define BTTN_AI 4
#define BTTN_SMS 35

// Supabase
#define supabaseUrl       "https://jursmglsfqaqrxvirtiw.supabase.co"
#define supabaseKey       "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Imp1cnNtZ2xzZnFhcXJ4dmlydGl3Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NDQ3ODkxOTEsImV4cCI6MjA2MDM2NTE5MX0.ajGbf9fLrYAA0KXzYhGFCTju-d4h-iTYTeU5WfITj3k"
#define tableName         "resident_number"

// AI suggestions using Gemini AI
const char *weatherApiKey = "7970309436bc52d518c7e71e314b8053";
const char *geminiApiKey = "AIzaSyD_g_WAsPqPKxltdOJt8VZw4uu359D3XXA";

// Create Audio object
Audio audio;

float fallback_latitude = 14.7160;
float fallback_longitude = 121.0615;

float latitude = 0.0;
float longitude = 0.0;

String location = "";
String weatherDescription = "";
float temperature = 0.0;
float feelsLike = 0.0;
float humidity = 0.0;

String AISuggestion = "";

#define TTS_GOOGLE_LANGUAGE "en"     // Use "tl" for Tagalog

// Dual timer configuration
#define LED_HOLD_TIME 500       // Minimum time (ms) before LED can change (debounce)
#define LED_ON_DURATION 20000   // Time LED stays on once activated (20 seconds)

unsigned long lastLedChangeTime = 0;  // For tracking LED hold time (debounce)
unsigned long ledActivationTime = 0;  // For tracking how long LED has been active
int currentLedState = 0;    // 0: none, 1: LED_ONE, 2: LED_TWO, 3: LED_THREE
bool ledActivated = false;  // Whether any LED is currently in its 20-second active period

int lastButtonState = HIGH;

// Sentence chunks
#define MAX_CHUNKS 10
String sentenceChunks[MAX_CHUNKS];
uint8_t numChunks = 0;

unsigned long lastPlayTime = 0;
const unsigned long playInterval = 60000UL;

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

  // Set microSD Card CS as OUTPUT and set HIGH
  pinMode(SD_CS, OUTPUT);      
  digitalWrite(SD_CS, HIGH); 

  // Initialize SPI bus for microSD Card
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  // Initialize microSD card with custom SPI
  if (!SD.begin(SD_CS, SPI)) {
    Serial.println("Error accessing microSD card!");
    while (true); 
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

  pinMode(BTTN_AI, INPUT_PULLUP);
  pinMode(BTTN_SMS, INPUT_PULLUP);
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
  if (distance <= 10) {
    targetLedState = 3;  // LED_THREE
  } 
  else if (distance <= 20) {
    targetLedState = 2;  // LED_TWO
  } 
  else if (distance <= 40) {
    targetLedState = 1;  // LED_ONE
  } 
  else {
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
  else if (ledActivated && targetLedState != 0 && targetLedState != currentLedState && 
          currentTime - lastLedChangeTime >= LED_HOLD_TIME) {
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

  if (digitalRead(BTTN_AI) == LOW) {
    Serial.println("AI button pressed!");

    audio.loop();
    audio.connecttoFS(SD, "AI-NOTIF.mp3");

    if (getLocationFromIpInfo() || (latitude = fallback_latitude, longitude = fallback_longitude, true)) {
      if (getWeather()) {

        getAISuggestion();

        Serial.print("AISuggestion: ");
        Serial.println(AISuggestion);

        playFloodWarning();
        lastPlayTime = millis();
        delay(1000); // debounce delay
      }
    }

  }

  audio.loop();
}

// 🔊 Speak in smart chunks
void speakTextInChunks(String text, int maxLength) {
  // Use a smaller chunk size
  int chunkSize = 60; // Reduced from 100
  
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
        end = punctEnd + 1; // Include the punctuation
      } else {
        // Otherwise find a space
        while (end > start && text[end] != ' ') {
          end--;
        }
        if (end == start) {
          end = start + chunkSize; // Worst case, just cut at max length
        }
      }
    }
    
    String chunk = text.substring(start, end);
    chunk.trim(); // Remove any leading/trailing spaces
    
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
  speakTextInChunks(AISuggestion, 100); // Split into chunks of ~100 characters
}

// Function to update LEDs based on the state
void updateLEDs(int state) {
  // Turn all LEDs off first
  turnOffAllLEDs();
  
  // Then turn on the appropriate LED based on state
  switch (state) {
    case 1:
      digitalWrite(LED_ONE, HIGH);
      audio.connecttoFS(SD, "LOW-FLOOD.mp3");
      break;
    case 2:
      digitalWrite(LED_TWO, HIGH);
      audio.connecttoFS(SD, "MEDIUM-FLOOD.mp3");
      break;
    case 3:
      digitalWrite(LED_THREE, HIGH);
      audio.connecttoFS(SD, "HIGH-FLOOD.mp3");
      break;
    // case 0 or default: all LEDs remain off
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

  String url = "http://api.openweathermap.org/data/2.5/weather?lat=" + String(latitude, 6) + "&lon=" + String(longitude, 6) + "&appid=" + weatherApiKey + "&units=metric&lang=en";

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
    
    if (!error && responseDoc.containsKey("candidates") && 
        responseDoc["candidates"][0].containsKey("content") && 
        responseDoc["candidates"][0]["content"].containsKey("parts") &&
        responseDoc["candidates"][0]["content"]["parts"][0].containsKey("text")) {
      
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


