/*
 * ESP32 Button SMS Sender with FreeRTOS, Supabase and Weather Integration
 * 
 * This program uses an ESP32 with FreeRTOS to send SMS messages when a button is pressed
 * Uses the HttpSMS API for message delivery, fetches recipient numbers from Supabase,
 * and includes current weather information in the message
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_task_wdt.h>  // Include ESP32 Task Watchdog Timer header
#include <LiquidCrystal_I2C.h>
#include "Arduino.h"
#include "Audio.h"
#include "SD.h"
#include "FS.h"
#include "SPI.h"

// Define TTS language
#define TTS_GOOGLE_LANGUAGE "tl-PH" // Tagalog (Philippines)

// WiFi credentials
const char* ssid = "TK-gacura";
const char* password = "gisaniel924";

// HC-SR04 Sensor pins
#define TRIG_PIN 17
#define ECHO_PIN 16

// LED pins
#define LED_ONE 13
#define LED_TWO 12
#define LED_THREE 14

// microSD Card Reader connections
#define SD_CS          5
#define SPI_MOSI      23 
#define SPI_MISO      19
#define SPI_SCK       18

// I2S Connections (MAX98357)
#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26

// Constants
#define SOUND_SPEED 0.034  // Sound speed in cm/uS
#define DISTANCE_READ_INTERVAL 100  // ms
#define LED_UPDATE_INTERVAL 50      // ms
#define LCD_UPDATE_INTERVAL 4000    // ms - reduced for more responsive updates
#define LED_TIMEOUT_MINUTES 10      // LED stays on for 10 minutes
#define LED_TIMEOUT_MS (LED_TIMEOUT_MINUTES * 60 * 1000)  // 10 minutes in milliseconds
#define DEBOUNCE_TIME_MS 5000       // 5 seconds debounce to prevent rapid changes
#define LCD_LOCK_TIME_MS 5000       // 5 seconds to lock LCD text after level detection

// Global variables for sharing data between tasks
volatile float currentDistance = 0;
SemaphoreHandle_t distanceMutex;

// LCD and display control variables
bool lcdTextLocked = false;
unsigned long lcdLockStartTime = 0;
unsigned long lastLCDUpdateTime = 0;
String currentDisplayedStatus = "Normal";
String weather = ""; // Weather string for display

// Create Audio object
Audio audio;

// LCD Setup - Assuming standard 16x2 I2C LCD at address 0x27
// Adjust address if your LCD uses a different one
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Timeout and state management - shared between LED and LCD
unsigned long stateLastChangeTime = 0;  // Shared timer for both LED and LCD
unsigned long lastStateUpdateTime = 0;  // For debounce
int lastDetectedRange = 0;  // 0=no detection, 1=far, 2=medium, 3=close
int activeState = 0;        // Current active state for both LED and LCD
bool timeoutEnabled = true;

// Task function prototypes
void readDistanceTask(void *parameter);
void controlOutputsTask(void *parameter);
void createWaterLevelSms(int waterLevel, float distance);
void sendSmsToAllNumbers();

// HttpSMS API key
const char* httpSmsApiKey = "MNJmgF7kRvUrTfj4fqDUbrzwoVFpMToWdTbiUx3sQ6jreYnbnu7bym-rQG3kB8_U";

// Supabase configuration
const char* supabaseUrl = "https://jursmglsfqaqrxvirtiw.supabase.co";
const char* supabaseKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Imp1cnNtZ2xzZnFhcXJ4dmlydGl3Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NDQ3ODkxOTEsImV4cCI6MjA2MDM2NTE5MX0.ajGbf9fLrYAA0KXzYhGFCTju-d4h-iTYTeU5WfITj3k";
const char* tableName = "resident_number";

// Weather API configuration
const char* weatherApiKey = "7970309436bc52d518c7e71e314b8053";
const char* geminiApiKey = "AIzaSyD_g_WAsPqPKxltdOJt8VZw4uu359D3XXA";

// Fallback coordinates for STI College Fairview
float fallback_latitude = 14.676208;
float fallback_longitude = 121.043861;

// Global variables for weather data
float latitude = 0;  // Initialize with fallback coordinates
float longitude = 0;
const String location = "New York";
String weatherDescription = "";  // Set default values
float temperature = 30.0;
float feelsLike = 32.0;
float humidity = 70.0;
String cityName = "Caloocan"; // Default city
bool weatherInitialized = false;
String aiWeatherMessage = "Sa kasalukuyan, walang banta ng baha sa Caloocan. Ang panahon ay maaliwalas, na may temperaturang 30.0°C, ngunit dahil sa 70% na halumigmig (humidity), mas ramdam and init na umaabot sa 32.0°C. Pinapayuhan ang lahat na magsuot ng magagaan at preskong damit at uminom ng maraming tubig upang makaiwas sa epekto ng matinding init.";

// SMS configuration
const char* smsFrom = "+639649687066"; // Your sender number or name
String phoneNumbers[10]; // Array to store up to 10 phone numbers
int numPhoneNumbers = 0;
char smsBody[1024]; // Buffer for dynamic SMS content

// Button configuration
#define BTTN_SMS 2                  // Button connected to GPIO pin 2
#define BTTN_AI 4
#define DEBOUNCE_DELAY 50          // Reduced debounce time to 50ms for faster response

// FreeRTOS handles
TaskHandle_t buttonTaskHandle = NULL;
TaskHandle_t wifiTaskHandle = NULL;
TaskHandle_t databaseTaskHandle = NULL;
TaskHandle_t weatherTaskHandle = NULL;
QueueHandle_t smsQueue = NULL;
SemaphoreHandle_t phoneNumbersMutex = NULL;
SemaphoreHandle_t weatherDataMutex = NULL;

// Global variables for SMS control
unsigned long lastSmsTime = 0;
#define SMS_COOLDOWN_MS 300000  // 5 minutes cooldown between SMS for the same level

/**
 * Task to read distance from HC-SR04 sensor
 */
void readDistanceTask(void *parameter) {
  float distance;
 
  while(true) {
    // Clears the TRIG_PIN
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    
    // Sets the TRIG_PIN HIGH for 10 microseconds
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    
    // Reads the ECHO_PIN, returns the sound wave travel time in microseconds
    float duration = pulseIn(ECHO_PIN, HIGH);
    
    // Calculate the distance
    distance = duration * SOUND_SPEED / 2;
    
    // Print the distance on the Serial Monitor
    // Serial.print("Distance: ");
    // Serial.print(distance);
    // Serial.println(" cm");
    
    // Update the shared distance variable with mutex protection
    if (xSemaphoreTake(distanceMutex, portMAX_DELAY) == pdTRUE) {
      currentDistance = distance;
      xSemaphoreGive(distanceMutex);
    }
    
    // Wait before next reading
    vTaskDelay(DISTANCE_READ_INTERVAL / portTICK_PERIOD_MS);
  }
}

/**
 * Combined task to control both LEDs and LCD based on the measured distance
 */
void controlOutputsTask(void *parameter) {
  float distance;
  int currentRange = 0;  // Current detected distance range
  unsigned long currentTime;
  String statusMessage = "";
  int lastSmsLevel = 0;  // Track the last level that triggered an SMS
 
  while(true) {
    currentTime = millis();
    
    // Get the current distance with mutex protection
    if (xSemaphoreTake(distanceMutex, portMAX_DELAY) == pdTRUE) {
      distance = currentDistance;
      xSemaphoreGive(distanceMutex);
    }
    
    // Determine current range based on distance
    // Only consider valid levels (no "normal" state)
    if (distance <= 15) {
      currentRange = 3;  // Close range - Warning
      statusMessage = "Warning";
    } else if (distance <= 25) {
      currentRange = 2;  // Medium range - Critical
      statusMessage = "Critical";
    } else if (distance <= 40) {
      currentRange = 1;  // Far range - Alert
      statusMessage = "Alert";
    } else {
      currentRange = 0;  // Out of range - ignore
      statusMessage = "Normal";
    }
    
    // Check if LCD text lock should be released
    if (lcdTextLocked && (currentTime - lcdLockStartTime) >= LCD_LOCK_TIME_MS) {
      lcdTextLocked = false;
      Serial.println("LCD text lock released");
      
      // When LCD lock is released, return to showing location/weather
      // but keep LEDs on until timeout
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Loc: ");
      lcd.print(location);
      
      lcd.setCursor(0, 1);
      lcd.print(weatherDescription);
      lcd.print(" ");
      lcd.print(temperature);
      
      lastLCDUpdateTime = currentTime; // Reset LCD update timer
    }
    
    // Update LCD with current information
    if (!lcdTextLocked && currentTime - lastLCDUpdateTime >= LCD_UPDATE_INTERVAL) {
      // Clear LCD for clean display
      lcd.clear();
      
      // Always show location and weather on LCD when not locked
      lcd.setCursor(0, 0);
      lcd.print("Loc: ");
      lcd.print(location);
      
      lcd.setCursor(0, 1);
      lcd.print(weatherDescription);
      lcd.print(" ");
      lcd.print(temperature);
      currentDisplayedStatus = "Normal";
      
      lastLCDUpdateTime = currentTime;
    }
    
    // Check if a valid range is detected and if debounce period has passed
    if (currentRange != lastDetectedRange && 
        (currentTime - lastStateUpdateTime) >= DEBOUNCE_TIME_MS) {
      // New valid range detected and debounce time passed
      lastDetectedRange = currentRange;
      lastStateUpdateTime = currentTime; // Update debounce timer
      
      if (currentRange > 0) {
        // Water level detected
        activeState = currentRange;
        stateLastChangeTime = currentTime; // Reset 10-minute timer
        
        // Update LEDs based on new range
        digitalWrite(LED_ONE, currentRange == 1 ? HIGH : LOW);
        digitalWrite(LED_TWO, currentRange == 2 ? HIGH : LOW);
        digitalWrite(LED_THREE, currentRange == 3 ? HIGH : LOW);
        
        // Play the appropriate alert sound based on water level - with safety checks
        if (SD.exists("/LOW-FLOOD-HIGH.mp3") && SD.exists("/MEDIUM-FLOOD-HIGH2.mp3") && SD.exists("/HIGH-FLOOD-HIGH2.mp3")) {
          switch (currentRange) {
            case 1:  // Alert (far)
              Serial.println("Playing alert sound");
              if (!audio.isRunning()) {  // Only start a new file if not already playing
                audio.connecttoFS(SD, "/LOW-FLOOD-HIGH.mp3");
              }
              break;
            case 2:  // Critical (medium)
              Serial.println("Playing critical sound");
              if (!audio.isRunning()) {  // Only start a new file if not already playing
                audio.connecttoFS(SD, "/MEDIUM-FLOOD-HIGH2.mp3");
              }
              break;
            case 3:  // Warning (close)
              Serial.println("Playing warning sound");
              if (!audio.isRunning()) {  // Only start a new file if not already playing
                audio.connecttoFS(SD, "/HIGH-FLOOD-HIGH2.mp3");
              }
              break;
          }
        } else {
          Serial.println("Alert sound files not found on SD card");
        }
        
        // Create and send custom SMS for the detected water level
        // Only send if it's a different level than the last SMS or if cooldown period has passed
        if ((currentRange != lastSmsLevel) || (currentTime - lastSmsTime >= SMS_COOLDOWN_MS)) {
          createWaterLevelSms(currentRange, distance);
          sendSmsToAllNumbers();
          lastSmsLevel = currentRange;
          lastSmsTime = currentTime;
          Serial.print("SMS sent for water level: ");
          Serial.println(currentRange);
        } else {
          Serial.println("SMS cooldown active. Not sending another SMS for the same level yet.");
        }
        
        // Lock the LCD text for 5 seconds
        lcdTextLocked = true;
        lcdLockStartTime = currentTime;
        
        // Force update the LCD immediately with the water level info
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Water Dis: ");
        lcd.print((int)distance);
        lcd.print("cm");
        
        lcd.setCursor(0, 1);
        lcd.print("Status: ");
        lcd.print(statusMessage);
        currentDisplayedStatus = statusMessage;
        
        Serial.print("New range detected: ");
        Serial.println(currentRange);
        Serial.print("Status: ");
        Serial.println(statusMessage);
        Serial.println("Timer reset to 10 minutes");
        Serial.println("LCD text locked for 5 seconds");
      } else {
        // No water level detected
        // Only turn off LEDs if no active state or timeout has expired
        if (activeState == 0 || (currentTime - stateLastChangeTime) >= LED_TIMEOUT_MS) {
          digitalWrite(LED_ONE, LOW);
          digitalWrite(LED_TWO, LOW);
          digitalWrite(LED_THREE, LOW);
          activeState = 0;
        }
        
        // Update LCD with location and weather
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Loc: ");
        lcd.print(location);
        
        lcd.setCursor(0, 1);
        lcd.print(weatherDescription);
        lcd.print(" ");
        lcd.print(temperature);
        
        Serial.println("No water level detected");
      }
    } else if (currentRange != lastDetectedRange) {
      // Range changed but within debounce period - ignore the change
      Serial.print("Ignoring change to range ");
      Serial.print(currentRange);
      Serial.println(" - debounce period active");
    }
    
    // Check if timer has expired and outputs should be turned off
    if (timeoutEnabled && activeState > 0 && (currentTime - stateLastChangeTime) >= LED_TIMEOUT_MS) {
      // Turn off all LEDs
      digitalWrite(LED_ONE, LOW);
      digitalWrite(LED_TWO, LOW);
      digitalWrite(LED_THREE, LOW);
      
      // Update LCD with location and weather if not already showing
      if (currentDisplayedStatus != "Normal") {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Loc: ");
        lcd.print(location);
        
        lcd.setCursor(0, 1);
        lcd.print(weatherDescription);
        lcd.print(" ");
        lcd.print(temperature);
        
        currentDisplayedStatus = "Normal";
      }
      
      // Update state
      activeState = 0;
      lcdTextLocked = false; // Make sure lock is released
      
      Serial.println("Timeout reached (10 minutes) - All outputs turned off");
    }
    
    // Wait before next update
    vTaskDelay(LED_UPDATE_INTERVAL / portTICK_PERIOD_MS);
  }
}

// Function to get location from IP info
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

// Function to get weather data
bool getWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Cannot fetch weather.");
    return false;
  }
  
  HTTPClient http;
  bool success = false;

  String url = "http://api.openweathermap.org/data/2.5/weather?lat=" + String(latitude, 6) + 
               "&lon=" + String(longitude, 6) + "&appid=" + weatherApiKey + "&units=metric&lang=en";

  Serial.println("Weather API URL: " + url);
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("OpenWeather Response: " + payload);

    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      // Take mutex before updating shared weather data
      if (xSemaphoreTake(weatherDataMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (doc.containsKey("name")) {
          cityName = doc["name"].as<String>();
        }

        if (doc.containsKey("weather") && doc["weather"][0].containsKey("description")) {
          weatherDescription = doc["weather"][0]["description"].as<String>();
        } else {
          // Keep existing value
          Serial.println("No weather description in response, keeping current value");
        }

        if (doc.containsKey("main")) {
          if (doc["main"].containsKey("temp")) {
            temperature = doc["main"]["temp"].as<float>();
          }
          if (doc["main"].containsKey("feels_like")) {
            feelsLike = doc["main"]["feels_like"].as<float>();
          }
          if (doc["main"].containsKey("humidity")) {
            humidity = doc["main"]["humidity"].as<float>();
          }
        } else {
          Serial.println("No main data in weather response");
        }

        xSemaphoreGive(weatherDataMutex);
      }

      Serial.println("==== WEATHER INFORMATION ====");
      Serial.print("Location: ");
      Serial.println(cityName);
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
      Serial.print("Error parsing weather data: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.println("Failed to connect to OpenWeather API, HTTP code: " + String(httpCode));
  }
  http.end();
  
  return success;
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
  speakTextInChunks(aiWeatherMessage, 100);  // Split into chunks of ~100 characters
}

// Function to get AI weather suggestion from Gemini
bool getAISuggestion() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Cannot fetch AI suggestion.");
    return false;
  }
  
  HTTPClient http;
  bool success = false;
  
  // Create a shorter prompt to reduce memory usage
  String prompt = "Weather update for " + cityName + ": " + weatherDescription + 
                  ", " + String(temperature, 1) + "°C (feels like " + 
                  String(feelsLike, 1) + "°C), humidity " + String(humidity, 0) + "%. ";
  
  prompt += "Write a 2-3 sentence message in Tagalog that: 1) Starts with 'PRAF Technology Weather Update:' ";
  prompt += "2) Includes flood risk assessment 3) Describes current weather 4) Gives a safety tip.";
  
  // Use a more memory-efficient approach with smaller JSON document
  DynamicJsonDocument requestDoc(1024);  // Reduced size
  JsonArray contents = requestDoc.createNestedArray("contents");
  JsonObject content = contents.createNestedObject();
  JsonArray parts = content.createNestedArray("parts");
  JsonObject part = parts.createNestedObject();
  part["text"] = prompt;
  
  JsonObject generationConfig = requestDoc.createNestedObject("generationConfig");
  generationConfig["temperature"] = 0.7;
  generationConfig["maxOutputTokens"] = 150;  // Reduced token count
  
  String requestBody;
  serializeJson(requestDoc, requestBody);
  requestDoc.clear();  // Free memory as soon as possible
  
  String geminiUrl = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key=" + String(geminiApiKey);
  
  Serial.println("Sending AI request...");
  http.begin(geminiUrl);
  http.addHeader("Content-Type", "application/json");
  
  int httpCode = http.POST(requestBody);
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // Create a new JSON document for parsing the response
    DynamicJsonDocument responseDoc(1024);  // Reduced size
    DeserializationError error = deserializeJson(responseDoc, payload);
    
    if (!error && responseDoc.containsKey("candidates") && 
        responseDoc["candidates"][0].containsKey("content") && 
        responseDoc["candidates"][0]["content"].containsKey("parts") &&
        responseDoc["candidates"][0]["content"]["parts"][0].containsKey("text")) {
      
      String aiResponse = responseDoc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
      
      if (xSemaphoreTake(weatherDataMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        aiWeatherMessage = aiResponse;
        xSemaphoreGive(weatherDataMutex);
      }
      
      Serial.println("\n==== AI WEATHER SUGGESTION ====");
      Serial.println(aiWeatherMessage);
      Serial.println("===============================\n");
      
      success = true;
    } else {
      Serial.println("Error parsing Gemini API response");
    }
    responseDoc.clear();  // Free memory as soon as possible
  } else {
    Serial.print("Failed to connect to Gemini API, HTTP code: ");
    Serial.println(httpCode);
  }
  
  http.end();
  return success;
}

// Function to update SMS body with current weather data and AI suggestion
void updateSmsBody() {
  if (xSemaphoreTake(weatherDataMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    // Make sure we have valid-looking values
    String localWeatherDesc = weatherDescription.length() > 0 ? weatherDescription : "cloudy";
    float localTemp = temperature != 0.0 ? temperature : 30.0;
    float localFeelsLike = feelsLike != 0.0 ? feelsLike : 32.0;
    float localHumidity = humidity != 0.0 ? humidity : 70.0;
    String localAIMessage = aiWeatherMessage.length() > 0 ? aiWeatherMessage : 
      "PRAF Technology Weather Update: Sa kasalukuyan, walang banta ng baha sa " + cityName + 
      ". Ang panahon ay " + localWeatherDesc + ", na may temperaturang " + String(localTemp, 2) + 
      "°C. Pinapayuhan ang lahat na uminom ng maraming tubig at manatiling malamig.";
    
    snprintf(smsBody, sizeof(smsBody),
      "🚨 FLOOD ALERT! 🚨\n\n"
      "📱 System Check:\n\n"
      "📍 Location: %s\n"
      "🌤️ Weather: %s\n"
      "🌡️ Temperature: %.1f°C\n"
      "🌡️ Feels like: %.1f°C\n"
      "💧 Humidity: %.0f%%\n\n"
      "🤖 AI Weather Update:\n"
      "%s\n\n"
      "From: PRAF Technology",
      cityName.c_str(), localWeatherDesc.c_str(), localTemp, localFeelsLike, localHumidity,
      localAIMessage.c_str()
    );
    
    // Print SMS body for debugging
    Serial.println("Updated SMS body:");
    Serial.println(smsBody);
    
    xSemaphoreGive(weatherDataMutex);
  }
}

// Function to fetch phone numbers from Supabase
void fetchPhoneNumbers() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Cannot fetch phone numbers.");
    return;
  }

  HTTPClient http;
  String endpoint = String(supabaseUrl) + "/rest/v1/" + tableName + "?select=number";
  
  http.begin(endpoint);
  http.addHeader("apikey", supabaseKey);
  http.addHeader("Authorization", "Bearer " + String(supabaseKey));
  http.addHeader("Content-Type", "application/json");
  
  int httpResponseCode = http.GET();
  
  if (httpResponseCode == 200) {
    String response = http.getString();
    Serial.println("Phone numbers received:");
    
    // Parse JSON response
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, response);
    
    if (error) {
      Serial.print("JSON deserialization failed: ");
      Serial.println(error.c_str());
    } else {
      // Process phone numbers
      JsonArray array = doc.as<JsonArray>();
      
      // Take the mutex to update the phone numbers array
      if (xSemaphoreTake(phoneNumbersMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        numPhoneNumbers = 0; // Reset counter
        
        for (JsonVariant entry : array) {
          if (numPhoneNumbers < 10) { // Limit to array size
            String number = entry["number"].as<String>();
            phoneNumbers[numPhoneNumbers] = number;
            
            Serial.print("Number ");
            Serial.print(numPhoneNumbers + 1);
            Serial.print(": ");
            Serial.println(number);
            
            numPhoneNumbers++;
          }
        }
        
        Serial.print("Found ");
        Serial.print(numPhoneNumbers);
        Serial.println(" phone numbers.");
        
        xSemaphoreGive(phoneNumbersMutex);
      }
    }
  } else {
    Serial.print("Error getting phone numbers. HTTP Response code: ");
    Serial.println(httpResponseCode);
    String response = http.getString();
    Serial.println("Response: " + response);
  }
  
  http.end();
}

// Function to send an SMS via HTTP
void sendHttpSMS(const char* from, const char* to, const char* body) {
  Serial.print("Sending SMS to: ");
  Serial.println(to);
  
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10); // Set a lower timeout to prevent blocking too long
  
  Serial.println("Connecting to SMS API...");
  int retry = 0;
  while (!client.connect("api.httpsms.com", 443) && retry < 3) {
    Serial.println("Connection to HttpSMS API failed, retrying...");
    vTaskDelay(pdMS_TO_TICKS(100)); // Yield to prevent watchdog trigger
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
  // Allow task to yield before reading response
  vTaskDelay(pdMS_TO_TICKS(10));
  
  // Read and print the response with proper yielding
  Serial.println("Reading SMS API response:");
  unsigned long timeout = millis();
  while ((client.connected() || client.available()) && millis() - timeout < 5000) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      Serial.println(line);
    } else {
      // Yield to prevent watchdog trigger when waiting for data
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
  
  client.stop();
  Serial.println("SMS Connection closed");
}

// Weather data fetch task
void weatherTask(void *pvParameters) {
  // Configure task so that it doesn't use the watchdog
  esp_task_wdt_delete(NULL); // Remove current task from WDT watch
  
  // Add a small delay to ensure proper task initialization
  vTaskDelay(pdMS_TO_TICKS(1000));
  
  // Get initial location and weather data
  int retries = 0;
  while (!getLocationFromIpInfo() && retries < 3) {
    Serial.println("Retrying location fetch...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    retries++;
  }
  
  if (retries >= 3) {
    Serial.println("Could not get location from IP. Using fallback coordinates.");
    latitude = fallback_latitude;
    longitude = fallback_longitude;
  }
  
  retries = 0;
  while (!getWeather() && retries < 3) {
    Serial.println("Retrying weather fetch...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    retries++;
  }
  
  if (retries < 3) {
    Serial.println("Weather data fetch successful!");
    // Get AI suggestion with a delay to avoid stack issues
    vTaskDelay(pdMS_TO_TICKS(1000));
    if (getAISuggestion()) {
      Serial.println("AI suggestion fetch successful!");
    } else {
      Serial.println("AI suggestion fetch failed. Using default message.");
    }
  } else {
    Serial.println("Could not fetch weather data. Using default values.");
  }
  
  // Always update SMS body, even with default values if data fetch failed
  updateSmsBody();
  weatherInitialized = true;
  
  while (1) {
    if (WiFi.status() == WL_CONNECTED) {
      bool locationUpdated = false;
      bool weatherUpdated = false;
      
      // Try to update location first
      if (getLocationFromIpInfo()) {
        locationUpdated = true;
        Serial.println("Location data updated");
      } else {
        Serial.println("Location update failed. Using previous coordinates.");
      }
      
      // Only update weather if location was updated or it's time for a refresh
      if (locationUpdated || !weatherUpdated) {
        if (getWeather()) {
          weatherUpdated = true;
          Serial.println("Weather data updated");
          // Add a small delay before making the AI call
          vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
          Serial.println("Weather update failed. Keeping previous values.");
        }
        
        // Get updated AI suggestion only if weather was updated successfully
        if (weatherUpdated) {
          if (getAISuggestion()) {
            Serial.println("AI suggestion updated");
          } else {
            Serial.println("AI suggestion update failed. Keeping previous message.");
          }
          
          updateSmsBody();
          Serial.println("SMS body updated with new data");
        }
      }
    } else {
      Serial.println("WiFi disconnected. Cannot update weather.");
    }
    // Update weather every 15 minutes (900000 ms)
    vTaskDelay(pdMS_TO_TICKS(900000));
  }
}

// Button monitoring task
void buttonTask(void *pvParameters) {
  // Configure task so that it doesn't use the watchdog
  esp_task_wdt_delete(NULL); // Remove current task from WDT watch
  
  int lastButtonState = HIGH;
  unsigned long lastDebounceTime = 0;
  
  while (1) {
    int reading = digitalRead(BTTN_SMS);
    
    // If button state changed
    if (reading != lastButtonState) {
      lastDebounceTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
    }

    if (digitalRead(BTTN_AI) == LOW) {
      Serial.println("AI button pressed!");

      playFloodWarning();  // Optional sound or alert before AI suggestion
    }
    
    // If enough time has passed since last state change
    if ((xTaskGetTickCount() * portTICK_PERIOD_MS - lastDebounceTime) > DEBOUNCE_DELAY) {
      // If button is pressed (LOW)
      if (reading == LOW) {
        Serial.println("Button pressed! Queueing SMS...");
        // Send a message to the queue
        int signalValue = 1;
        xQueueSend(smsQueue, &signalValue, 0);
        
        // Wait for button release to prevent multiple triggers
        while (digitalRead(BTTN_SMS) == LOW) {
          vTaskDelay(pdMS_TO_TICKS(10));
        }
      }
    }
    
    lastButtonState = reading;
    vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to prevent task starvation
  }
}

// Database task - periodically fetches phone numbers
void databaseTask(void *pvParameters) {
  // Configure task so that it doesn't use the watchdog
  esp_task_wdt_delete(NULL); // Remove current task from WDT watch
  
  while (1) {
    if (WiFi.status() == WL_CONNECTED) {
      fetchPhoneNumbers();
    }
    // Check every 5 seconds for new phone numbers
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

// WiFi and SMS handling task
void wifiTask(void *pvParameters) {
  // Configure task so that it doesn't use the watchdog
  esp_task_wdt_delete(NULL); // Remove current task from WDT watch
  
  int signalValue;
  
  // Initial fetch of phone numbers
  fetchPhoneNumbers();
  
  while (1) {
    if (xQueueReceive(smsQueue, &signalValue, portMAX_DELAY)) {
      if (WiFi.status() == WL_CONNECTED) {
        // Make sure SMS body is up-to-date with latest weather data
        updateSmsBody();
        
        // Take the mutex to access the phone numbers array
        if (xSemaphoreTake(phoneNumbersMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
          if (numPhoneNumbers > 0) {
            Serial.printf("Sending SMS to %d recipients...\n", numPhoneNumbers);
            
            // Send SMS to all phone numbers
            for (int i = 0; i < numPhoneNumbers; i++) {
              // Yield to prevent watchdog trigger
              vTaskDelay(pdMS_TO_TICKS(10));
              
              sendHttpSMS(smsFrom, phoneNumbers[i].c_str(), smsBody);
              // Increased delay between sending messages to give more time for system tasks
              vTaskDelay(pdMS_TO_TICKS(1000));
              
              // Explicitly yield to the scheduler
              taskYIELD();
            }
          } else {
            Serial.println("No phone numbers available. SMS not sent.");
          }
          xSemaphoreGive(phoneNumbersMutex);
        }
      } else {
        Serial.println("WiFi disconnected. Attempting to reconnect...");
        WiFi.begin(ssid, password);
      }
    }
  }
}

// Function to create custom SMS body for different water levels
void createWaterLevelSms(int waterLevel, float distance) {
  if (xSemaphoreTake(weatherDataMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    // Make sure we have valid-looking values
    String localWeatherDesc = weatherDescription.length() > 0 ? weatherDescription : "cloudy";
    float localTemp = temperature != 0.0 ? temperature : 30.0;
    float localFeelsLike = feelsLike != 0.0 ? feelsLike : 32.0;
    float localHumidity = humidity != 0.0 ? humidity : 70.0;
    
    String levelMessage;
    String levelEmoji;
    String actionRequired;
    
    // Create custom message based on water level
    switch (waterLevel) {
      case 1: // Alert (Low level)
        levelEmoji = "⚠️";
        levelMessage = "LOW LEVEL FLOOD ALERT";
        actionRequired = "Monitor water levels. Prepare emergency supplies. Stay informed.";
        break;
      case 2: // Critical (Medium level)
        levelEmoji = "🔴";
        levelMessage = "MEDIUM LEVEL FLOOD WARNING";
        actionRequired = "Move valuables to higher ground. Prepare for possible evacuation. Stay vigilant.";
        break;
      case 3: // Warning (High level)
        levelEmoji = "⛔";
        levelMessage = "HIGH LEVEL FLOOD EMERGENCY";
        actionRequired = "Evacuate immediately to designated safe zones. Follow emergency protocols. Avoid flooded areas.";
        break;
      default:
        levelEmoji = "ℹ️";
        levelMessage = "WATER LEVEL UPDATE";
        actionRequired = "No immediate action required. Stay informed.";
    }
    
    snprintf(smsBody, sizeof(smsBody),
      "%s %s %s\n\n"
      "📍 Location: %s\n"
      "💧 Water Level: %d cm\n"
      "🌤️ Weather: %s\n"
      "🌡️ Temperature: %.1f°C\n"
      "💧 Humidity: %.0f%%\n\n"
      "⚡ ACTION REQUIRED: %s\n\n"
      "From: PRAF Technology Flood Monitoring System",
      levelEmoji, levelMessage, levelEmoji,
      cityName.c_str(), (int)distance,
      localWeatherDesc.c_str(), localTemp, localHumidity,
      actionRequired.c_str()
    );
    
    // Print SMS body for debugging
    Serial.println("Created water level SMS:");
    Serial.println(smsBody);
    
    xSemaphoreGive(weatherDataMutex);
  }
}

// Function to send SMS to all registered numbers
void sendSmsToAllNumbers() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Cannot send SMS.");
    return;
  }
  
  // Take the mutex to access the phone numbers array
  if (xSemaphoreTake(phoneNumbersMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    if (numPhoneNumbers > 0) {
      Serial.printf("Sending water level SMS to %d recipients...\n", numPhoneNumbers);
      
      // Send SMS to all phone numbers
      for (int i = 0; i < numPhoneNumbers; i++) {
        // Yield to prevent watchdog trigger
        vTaskDelay(pdMS_TO_TICKS(10));
        
        sendHttpSMS(smsFrom, phoneNumbers[i].c_str(), smsBody);
        // Increased delay between sending messages to give more time for system tasks
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Explicitly yield to the scheduler
        taskYIELD();
      }
    } else {
      Serial.println("No phone numbers available. SMS not sent.");
    }
    xSemaphoreGive(phoneNumbersMutex);
  }
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);

  // Connect to WiFi
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.print(".");
  }
  Serial.println(" CONNECTED");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Wait for weather to be initialized before handling SMS
  int timeout = 0;
  while (!weatherInitialized && timeout < 30) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    timeout++;
  }
  
  if (!weatherInitialized) {
    Serial.println("Weather initialization timed out. SMS will use default values.");
    updateSmsBody(); // Create SMS with default values
  }

  // Initialize LCD
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Water Monitor");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(1000);

  Serial.println("\nESP32 Button SMS Sender with FreeRTOS, Supabase and Weather Integration");
  Serial.println("Automatic SMS alerts enabled for water level detection");
  
  // Configure HC-SR04 pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
 
  // Configure LED pins
  pinMode(LED_ONE, OUTPUT);
  pinMode(LED_TWO, OUTPUT);
  pinMode(LED_THREE, OUTPUT);
 
  // Initially turn off all LEDs
  digitalWrite(LED_ONE, LOW);
  digitalWrite(LED_TWO, LOW);
  digitalWrite(LED_THREE, LOW);
 
  // Create mutex for shared data protection
  distanceMutex = xSemaphoreCreateMutex();
  
  // Set button pin as input with internal pull-up resistor
  pinMode(BTTN_SMS, INPUT_PULLUP);
  
  // Create mutexes
  phoneNumbersMutex = xSemaphoreCreateMutex();
  weatherDataMutex = xSemaphoreCreateMutex();
  
  // Create queue for button events
  smsQueue = xQueueCreate(5, sizeof(int));

  // Create FreeRTOS tasks
  xTaskCreate(
    readDistanceTask,     // Task function
    "ReadDistance",       // Task name
    2048,                 // Stack size (bytes)
    NULL,                 // Task parameters
    1,                    // Priority (1 is low)
    NULL                  // Task handle
  );
 
  xTaskCreate(
    controlOutputsTask,   // Task function to control both LEDs and LCD
    "ControlOutputs",     // Task name
    2048,                 // Stack size (bytes)
    NULL,                 // Task parameters
    1,                    // Priority
    NULL                  // Task handle
  );

  // Create tasks - Note: we need to create tasks before configuring WDT
  xTaskCreate(
    buttonTask,      // Task function
    "ButtonTask",    // Task name
    2048,           // Stack size
    NULL,           // Task parameters
    1,              // Task priority
    &buttonTaskHandle
  );
  
  xTaskCreate(
    databaseTask,    // Task function
    "DatabaseTask",  // Task name
    4096,           // Stack size
    NULL,           // Task parameters
    1,              // Task priority
    &databaseTaskHandle
  );
  
  xTaskCreate(
    weatherTask,     // Task function
    "WeatherTask",   // Task name
    8192,           // Increased stack size from 4096 to 8192
    NULL,           // Task parameters
    1,              // Task priority
    &weatherTaskHandle
  );
  
  xTaskCreate(
    wifiTask,       // Task function
    "WifiTask",     // Task name
    4096,          // Stack size
    NULL,          // Task parameters
    2,             // Task priority
    &wifiTaskHandle
  );

  Serial.println("ESP32 HC-SR04 Distance Sensor with FreeRTOS Started");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Loc: ");
  lcd.print(location);
  lcd.setCursor(0, 1);
  
  // Initialize weatherDescription if empty
  if (weatherDescription.length() == 0) {
    weatherDescription = "clear sky";
  }
  
  lcd.print(weatherDescription);
  lcd.print(" ");
  lcd.print(temperature);
}

void loop() {
  // Empty loop as tasks are handled by FreeRTOS
  vTaskDelay(pdMS_TO_TICKS(1000));
}
