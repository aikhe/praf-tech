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
#include <SPI.h>
#include <SD.h>
#include <Audio.h>
// #include "FS.h"

// Pull in the ESP‐IDF I2S declarations
extern "C" {
  #include "driver/i2s.h"
}

// microSD Card Reader connections
#define SD_CS_PIN    5
#define SD_SCK_PIN  18
#define SD_MISO_PIN 19
#define SD_MOSI_PIN 23

#define SD_CS          5
#define SPI_MOSI      23 
#define SPI_MISO      19
#define SPI_SCK       18

// I2S Connections (MAX98357)
#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26

// Use VSPI (SPIClass HSPI would be SPIClass(HSPI); VSPI is default second bus)
SPIClass spiSD(VSPI);

// // Create Audio object
Audio audio;
// Audio *audio = nullptr;  // no global instantiation

// // Task handle for audio playback
// TaskHandle_t audioTaskHandle;
// 
// // Audio playback task
// void audioTask(void *parameter) {
//   while (true) {
//     audio.loop();
// 
//     if (!audio.isRunning()) {
//       Serial.println("Restarting audio...");
//       audio.connecttoFS(SD, "/DEVICE-START-VOICE.mp3");
//     }
// 
//     vTaskDelay(10 / portTICK_PERIOD_MS);
//   }
// }

// CHECKPOINT

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

// Constants
#define SOUND_SPEED 0.034  // Sound speed in cm/uS
#define DISTANCE_READ_INTERVAL 100  // ms
#define LED_UPDATE_INTERVAL 50      // ms
#define LCD_UPDATE_INTERVAL 500     // ms
#define LED_TIMEOUT_MINUTES 10      // LED stays on for 10 minutes
#define LED_TIMEOUT_MS (LED_TIMEOUT_MINUTES * 60 * 1000)  // 10 minutes in milliseconds
#define DEBOUNCE_TIME_MS 5000       // 5 seconds debounce to prevent rapid changes

// Global variables for sharing data between tasks
volatile float currentDistance = 0;
SemaphoreHandle_t distanceMutex;

// LCD Setup - Assuming standard 16x2 I2C LCD at address 0x27
// Adjust address if your LCD uses a different one
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Timeout and state management - shared between LED and LCD
unsigned long stateLastChangeTime = 0;  // Shared timer for both LED and LCD
unsigned long lastStateUpdateTime = 0;  // For debounce
int lastDetectedRange = 0;  // 0=no detection, 1=far, 2=medium, 3=close
int activeState = 0;        // Current active state for both LED and LCD
bool timeoutEnabled = true;
// Additional variables for improved LCD management
unsigned long lastLCDUpdateTime = 0;  // Track last LCD update time
String currentDisplayedStatus = "";   // Track last status display to avoid unnecessary updates
unsigned long lcdLockStartTime = 0;   // When the LCD text was locked
bool lcdTextLocked = false;           // Whether LCD is showing locked text
#define LCD_LOCK_TIME_MS 5000         // 5 seconds to lock LCD text after level detection
#define LCD_UPDATE_INTERVAL 4000      // ms - longer interval for normal updates

// Task function prototypes
void readDistanceTask(void *parameter);
void controlOutputsTask(void *parameter);

// SMS message function prototypes
void createAlertSMS(float distance);
void createCriticalSMS(float distance);
void createWarningSMS(float distance);

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
String weatherDescription = "";  // Set default values
float temperature = 30.0;
float feelsLike = 32.0;
float humidity = 70.0;
String cityName = "Caloocan"; // Default city
bool weatherInitialized = false;
String aiWeatherMessage = "Sa kasalukuyan, walang banta ng baha sa Caloocan. Ang panahon ay maaliwalas, na may temperaturang 30.0°C, ngunit dahil sa 70% na halumigmig (humidity), mas ramdam and init na umaabot sa 32.0°C. Pinapayuhan ang lahat na magsuot ng magagaan at preskong damit at uminom ng maraming tubig upang makaiwas sa epekto ng matinding init.";

// SMS configuration
const char* smsFrom = "+639649687066"; // Your sender number or name
String phoneNumbers[20]; // Array to store up to 10 phone numbers
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

// Recursively list a directory and its children
void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    for (uint8_t i = 0; i < levels; i++) {
      Serial.print("  ");
    }
    if (file.isDirectory()) {
      Serial.print("[DIR] ");
      Serial.println(file.name());
      // Recurse into sub-directory
      listDir(fs, file.name(), levels + 1);
    } else {
      Serial.print("[FILE] ");
      Serial.print(file.name());
      Serial.print("\tSIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

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
      
      // When LCD lock is released, return to showing weather information
      // but keep LEDs on until timeout
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(cityName);
      lcd.setCursor(0, 1);
      lcd.print(weatherDescription);
      lcd.print(" ");
      lcd.print((int)temperature);
      lcd.print("C");
      
      lastLCDUpdateTime = currentTime; // Reset LCD update timer
    }
    
    // Update LCD with current information if not locked and update interval passed
    if (!lcdTextLocked && currentTime - lastLCDUpdateTime >= LCD_UPDATE_INTERVAL) {
      // Clear LCD for clean display
      lcd.clear();
      
      // Show monitoring status on LCD when not locked
      lcd.setCursor(0, 0);
      lcd.print(cityName);
      lcd.setCursor(0, 1);
      lcd.print(weatherDescription);
      lcd.print(" ");
      lcd.print((int)temperature);
      lcd.print("C");
      currentDisplayedStatus = "Normal";
      
      lastLCDUpdateTime = currentTime;
    }
    
    // Check if a valid range is detected and if debounce period has passed
    if (currentRange != lastDetectedRange && currentRange > 0 && 
        (currentTime - lastStateUpdateTime) >= DEBOUNCE_TIME_MS) {
      // New valid range detected and debounce time passed
      lastDetectedRange = currentRange;
      lastStateUpdateTime = currentTime; // Update debounce timer
      stateLastChangeTime = currentTime; // Reset 10-minute timer
      activeState = currentRange;
      
      // Update LEDs based on new range
      digitalWrite(LED_ONE, currentRange == 1 ? HIGH : LOW);
      digitalWrite(LED_TWO, currentRange == 2 ? HIGH : LOW);
      digitalWrite(LED_THREE, currentRange == 3 ? HIGH : LOW);
      
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
      
      // Create appropriate SMS message based on flood level
      switch(currentRange) {
        case 1: // Alert
          createAlertSMS(distance);
          Serial.println("Created Alert SMS for Level 1");
          break;
        case 2: // Critical
          createCriticalSMS(distance);
          Serial.println("Created Critical SMS for Level 2");
          break;
        case 3: // Warning
          createWarningSMS(distance);
          Serial.println("Created Warning SMS for Level 3");
          break;
      }
      
      // Queue SMS to be sent - use a specific signal value for each level
      int signalValue = 10 + currentRange; // 11=Alert, 12=Critical, 13=Warning
      if (xQueueSend(smsQueue, &signalValue, 0) == pdTRUE) {
        Serial.printf("Queued SMS for flood level %d\n", currentRange);
      } else {
        Serial.println("Failed to queue SMS - queue might be full");
      }
      
    } else if (currentRange != lastDetectedRange && currentRange > 0) {
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
      
      // Update LCD with normal monitoring status if not already showing
      if (currentDisplayedStatus != "Normal") {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Loc: ");
        lcd.print(cityName);
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

// Function to create Alert level SMS (Level 1 - Far range)
void createAlertSMS(float distance) {
  if (xSemaphoreTake(weatherDataMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    // Make sure we have valid-looking values
    String localWeatherDesc = weatherDescription.length() > 0 ? weatherDescription : "cloudy";
    float localTemp = temperature != 0.0 ? temperature : 30.0;
    float localFeelsLike = feelsLike != 0.0 ? feelsLike : 32.0;
    float localHumidity = humidity != 0.0 ? humidity : 70.0;
    
    snprintf(smsBody, sizeof(smsBody),
      "⚠️ FLOOD ALERT - LEVEL 1 ⚠️\n\n"
      "📍 Location: %s\n"
      "🔍 Current water level: %.1f cm\n"
      "⚠️ Status: ALERT - Initial flooding detected\n\n"
      // "🌤️ Weather: %s\n"
      // "🌡️ Temperature: %.1f°C\n"
      // "💧 Humidity: %.0f%%\n\n"
      "🚨 PRECAUTIONS:\n"
      "- Monitor water levels\n"
      "- Prepare emergency supplies\n"
      "- Stay tuned for updates\n\n"
      "From: PRAF Technology Flood Monitoring System",
      cityName.c_str(), distance
    );
    
    xSemaphoreGive(weatherDataMutex);
  }
}

// Function to create Critical level SMS (Level 2 - Medium range)
void createCriticalSMS(float distance) {
  if (xSemaphoreTake(weatherDataMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    // Make sure we have valid-looking values
    String localWeatherDesc = weatherDescription.length() > 0 ? weatherDescription : "cloudy";
    float localTemp = temperature != 0.0 ? temperature : 30.0;
    float localFeelsLike = feelsLike != 0.0 ? feelsLike : 32.0;
    float localHumidity = humidity != 0.0 ? humidity : 70.0;
    
    snprintf(smsBody, sizeof(smsBody),
      "🔴 FLOOD CRITICAL - LEVEL 2 🔴\n\n"
      "📍 Location: %s\n"
      "🔍 Current water level: %.1f cm\n"
      "⚠️ Status: CRITICAL - Significant flooding\n\n"
      // "🌤️ Weather: %s\n"
      // "🌡️ Temperature: %.1f°C\n"
      // "💧 Humidity: %.0f%%\n\n"
      "🚨 URGENT ACTIONS REQUIRED:\n"
      "- Move valuables to higher ground\n"
      "- Prepare for possible evacuation\n"
      "- Avoid flooded areas\n"
      "- Charge communication devices\n\n"
      "From: PRAF Technology Flood Monitoring System",
      cityName.c_str(), distance
    );
    
    xSemaphoreGive(weatherDataMutex);
  }
}

// Function to create Warning level SMS (Level 3 - Close range)
void createWarningSMS(float distance) {
  if (xSemaphoreTake(weatherDataMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    // Make sure we have valid-looking values
    String localWeatherDesc = weatherDescription.length() > 0 ? weatherDescription : "cloudy";
    float localTemp = temperature != 0.0 ? temperature : 30.0;
    float localFeelsLike = feelsLike != 0.0 ? feelsLike : 32.0;
    float localHumidity = humidity != 0.0 ? humidity : 70.0;
    
    snprintf(smsBody, sizeof(smsBody),
      "🚨 FLOOD WARNING - LEVEL 3 🚨\n\n"
      "📍 Location: %s\n"
      "🔍 Current water level: %.1f cm\n"
      "⚠️ Status: WARNING - Severe flooding\n\n"
      // "🌤️ Weather: %s\n"
      // "🌡️ Temperature: %.1f°C\n"
      // "💧 Humidity: %.0f%%\n\n"
      "🚨 EMERGENCY ACTIONS REQUIRED:\n"
      "- EVACUATE immediately if instructed\n"
      "- Move to higher ground NOW\n"
      "- Follow emergency routes\n"
      "- Do NOT attempt to cross floodwaters\n"
      "- Call emergency services if trapped\n\n"
      "From: PRAF Technology Flood Monitoring System",
      cityName.c_str(), distance
    );
    
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
    int ai = digitalRead(BTTN_AI);
    
    // If button state changed
    if (reading != lastButtonState) {
      lastDebounceTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
    }

    
    // If enough time has passed since last state change
    if ((xTaskGetTickCount() * portTICK_PERIOD_MS - lastDebounceTime) > DEBOUNCE_DELAY) {
      // If button is pressed (LOW)
      if (ai == LOW) {
        Serial.println("Button pressed! AI SMS...");
        Serial.println(aiWeatherMessage);

        while (digitalRead(BTTN_AI) == LOW) {
          vTaskDelay(pdMS_TO_TICKS(10));
        }
      }

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
  
  while (1) {
    if (xQueueReceive(smsQueue, &signalValue, portMAX_DELAY)) {
      if (WiFi.status() == WL_CONNECTED) {
        // Determine what type of SMS to send based on signal value
        String alertType = "Standard";
        
        if (signalValue >= 11 && signalValue <= 13) {
          // This is a flood level alert (11=Alert, 12=Critical, 13=Warning)
          int floodLevel = signalValue - 10;
          
          switch(floodLevel) {
            case 1:
              alertType = "Alert (Level 1)";
              break;
            case 2:
              alertType = "Critical (Level 2)";
              break;
            case 3:
              alertType = "Warning (Level 3)";
              break;
          }
          
          Serial.printf("Preparing to send %s SMS alerts\n", alertType.c_str());
          // SMS body was already prepared in controlOutputsTask
        } else {
          // Standard SMS button press - update SMS body with current weather
          updateSmsBody();
          alertType = "Standard";
        }
        
        // Take the mutex to access the phone numbers array
        if (xSemaphoreTake(phoneNumbersMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
          if (numPhoneNumbers > 0) {
            Serial.printf("Sending %s SMS to %d recipients...\n", alertType.c_str(), numPhoneNumbers);
            
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
            
            Serial.printf("Finished sending %s SMS alerts\n", alertType.c_str());
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

void setup() {
  // Set microSD Card CS pin  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  // Initialize serial communication
  Serial.begin(115200);

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

  // ——— 1) Uninstall I2S before Wi-Fi comes up ———
  // This tears down both TX (DAC) and RX (ADC2) channels
  i2s_driver_uninstall(I2S_NUM_0);                 // frees ADC2 for Wi-Fi :contentReference[oaicite:1]{index=1}

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
  
  // Initialize LCD-related variables
  lastLCDUpdateTime = 0;
  currentDisplayedStatus = "Normal";
  lcdTextLocked = false;
  
  // Set button pin as input with internal pull-up resistor
  pinMode(BTTN_SMS, INPUT_PULLUP);
  pinMode(BTTN_AI, INPUT_PULLUP);
  
  // Create mutexes
  phoneNumbersMutex = xSemaphoreCreateMutex();
  weatherDataMutex = xSemaphoreCreateMutex();
  
  // Create queue for button events
  smsQueue = xQueueCreate(5, sizeof(int));

  spiSD.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (!SD.begin(SD_CS, spiSD)) {
    Serial.println("❌ SD init failed");
    while (1) delay(100);
  }
  Serial.println("✅ SD initialized");

  // // Initialize SPI for SD card
  // SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  // // Initialize SD card
  // if (!SD.begin(SD_CS, SPI)) {
  //   Serial.println("SD card initialization failed!");
  //   while (true);
  // }

  Serial.println("SD card initialized.");
  listDir(SD, "/", 0);

  // Set up I2S for audio output
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);

  // Set volume level (0–100)
  audio.setVolume(100);

  // // Play the initial audio file
  // audio.connecttoFS(SD, "/DEVICE-START-VOICE.mp3");

  // Create FreeRTOS task for audio playback
  // xTaskCreate(
  //   audioTask,           // Task function
  //   "AudioTask",         // Name
  //   4096,                // Stack size
  //   NULL,                // Parameter
  //   1,                   // Priority
  //   &audioTaskHandle     // Task handle
  // );

  // Create FreeRTOS tasks
  xTaskCreate(
    readDistanceTask,     // Task function
    "ReadDistance",       // Task name
    4096,                 // Stack size (bytes) - increased from 2048
    NULL,                 // Task parameters
    1,                    // Priority (1 is low)
    NULL                  // Task handle
  );
 
  xTaskCreate(
    controlOutputsTask,   // Task function to control both LEDs and LCD
    "ControlOutputs",     // Task name
    8192,                 // Stack size (bytes) - increased from 2048
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
}

void loop() {
  // Empty loop as tasks are handled by FreeRTOS
  vTaskDelay(pdMS_TO_TICKS(1000));
}
