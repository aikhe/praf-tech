/*
 * ESP32 Button SMS Sender with FreeRTOS, Supabase and Weather Integration
 * 
 * This program uses an ESP32 with FreeRTOS to send SMS messages when a button is pressed
 * Uses the HttpSMS API for message delivery, fetches recipient numbers from Supabase,
 * and includes current weather information in the message
 */

// ACTUALLY CHECKPOINT

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
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"

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

// Create instances for audio components
I2SStream i2sStream;
MP3DecoderHelix mp3Decoder;
EncodedAudioStream decoder(&i2sStream, &mp3Decoder);
File mp3File;
StreamCopy copier;

// TTS components
URLStream url; // Will be initialized after WiFi credentials are defined
EncodedAudioStream ttsDecoder(&i2sStream, &mp3Decoder);
StreamCopy ttsCopier(ttsDecoder, url);

// Google TTS URL template
const char* queryTemplate = "http://translate.google.com/translate_tts"
                            "?ie=UTF-8&tl=%1&client=tw-ob&ttsspeed=%2&q=%3";

// // Create Audio object
// Audio audio;
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
const char* ssid = "Doogy";
const char* password = "March136647";

// HC-SR04 Sensor pins
#define TRIG_PIN 17
#define ECHO_PIN 16

// LED pins
#define LED_ONE 13
#define LED_TWO 12
#define LED_THREE 14
#define AI_LED_ONE 32
#define AI_LED_TWO 15
#define AI_LED_THREE 33

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
volatile bool systemInitialized = false;  // Add initialization flag
volatile bool sensorStabilized = false;   // Flag to indicate sensor readings have stabilized
volatile int stabilizationReadings = 0;   // Counter for stabilization readings
unsigned long startupTime = 0;  // Track when system started

// Audio playback status and SMS delay
volatile bool audioPlaybackFinished = true; // Initialize to true since no audio is playing at startup
SemaphoreHandle_t audioStatusMutex;
TaskHandle_t audioTaskHandle = NULL;

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
#define LCD_LOCK_TIME_MS 60000         // 60 seconds (1 minute) to lock LCD text after level detection
#define LCD_UPDATE_INTERVAL 4000      // ms - longer interval for normal updates

// Task function prototypes
void readDistanceTask(void *parameter);
void controlOutputsTask(void *parameter);
void smsSendDelayTask(void *parameter);
void waitForAudioThenTTS(void *parameter);

// SMS message function prototypes
void createAlertSMS(float distance);
void createCriticalSMS(float distance);
void createWarningSMS(float distance);

// HttpSMS API key
const char* httpSmsApiKey = "uk_eLFe3Xme9Qn2jhXLt2bPA9b_xVWZMSlDDpRMmFOzl9CoAGdAe-fzRmmCXEZ5AkMa";

// Supabase configuration
const char* supabaseUrl = "https://jursmglsfqaqrxvirtiw.supabase.co";
const char* supabaseKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Imp1cnNtZ2xzZnFhcXJ4dmlydGl3Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NDQ3ODkxOTEsImV4cCI6MjA2MDM2NTE5MX0.ajGbf9fLrYAA0KXzYhGFCTju-d4h-iTYTeU5WfITj3k";
const char* tableName = "phone_numbers";

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
String phoneNames[20]; // Array to store corresponding names for personalization
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
TaskHandle_t ledTaskHandle = NULL;
QueueHandle_t smsQueue = NULL;
SemaphoreHandle_t phoneNumbersMutex = NULL;
SemaphoreHandle_t weatherDataMutex = NULL;

// TTS function prototypes
void ttsPlaybackTask(void* parameter);
String makeTTSUrl(const String& chunk, const char* lang, const char* speed);
void splitText(const String& text, size_t maxLen, std::vector<String>& outChunks);
String urlEncode(const String& text);

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
  // Initialize distance to a safe value that won't trigger alerts
  float safeDistance = 100.0;  // 100cm - well above any alert threshold
  
  // Update shared variable with safe initial value
  if (xSemaphoreTake(distanceMutex, portMAX_DELAY) == pdTRUE) {
    currentDistance = safeDistance;
    xSemaphoreGive(distanceMutex);
  }
 
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
    
    // Validate reading - ignore unreasonable values
    if (distance <= 0 || distance > 400) {  // 400cm is max range for HC-SR04
      // Serial.println("Invalid distance reading, ignoring");
      vTaskDelay(DISTANCE_READ_INTERVAL / portTICK_PERIOD_MS);
      continue;
    }
    
    // Update the shared distance variable with mutex protection
    if (xSemaphoreTake(distanceMutex, portMAX_DELAY) == pdTRUE) {
      currentDistance = distance;
      
      // If system is initialized but sensor not yet stabilized, count readings
      if (systemInitialized && !sensorStabilized) {
        stabilizationReadings++;
        // After 10 valid readings, consider sensor stabilized
        if (stabilizationReadings >= 10) {
          sensorStabilized = true;
          Serial.println("Sensor readings have stabilized");
        }
      }
      
      xSemaphoreGive(distanceMutex);
    }
    
    // Wait before next reading
    vTaskDelay(DISTANCE_READ_INTERVAL / portTICK_PERIOD_MS);
  }
}

/**
 * LED Animation Task
 */
void ledAnimationTask(void *parameter) {
  // Define sequence
  int leds[] = { AI_LED_ONE, AI_LED_TWO, AI_LED_THREE };
  int sequence[] = { 0, 1, 2, 0, 1, 2, -1 };  // -1 = all LEDs on
  int currentStep = 0;
  
  // Run LED sequence while audio is playing or animation not complete
  while (currentStep < 7) {
    // Turn off all LEDs before applying the next step
    for (int j = 0; j < 3; j++) {
      digitalWrite(leds[j], LOW);
    }
    
    if (currentStep < 7) {
      if (sequence[currentStep] == -1) {
        // Turn all LEDs ON
        vTaskDelay(pdMS_TO_TICKS(200));
        for (int j = 0; j < 3; j++) {
          digitalWrite(leds[j], HIGH);
        }

        vTaskDelay(pdMS_TO_TICKS(8000));
      } else {
        digitalWrite(leds[sequence[currentStep]], HIGH);
      }
      currentStep++;
    }
    
    vTaskDelay(pdMS_TO_TICKS(200));
  }
  
  // Turn off all LEDs when done
  for (int i = 0; i < 3; i++) {
    digitalWrite(leds[i], LOW);
  }
  
  // Delete the task
  vTaskDelete(NULL);
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
    
    // Check if system is initialized (wait 5 seconds after startup)
    if (!systemInitialized) {
      if (currentTime - startupTime >= 5000) {  // 5 second initialization period
        systemInitialized = true;
        Serial.println("System initialization complete - starting monitoring");
        
        // Clear LCD and show ready message
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("PRAF System");
        lcd.setCursor(0, 1);
        lcd.print("Monitoring...");
        delay(2000);  // Show ready message for 2 seconds
      } else {
        // During initialization, show countdown
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Initializing");
        lcd.setCursor(0, 1);
        lcd.print((5000 - (currentTime - startupTime)) / 1000);
        lcd.print(" seconds...");
        vTaskDelay(LED_UPDATE_INTERVAL / portTICK_PERIOD_MS);
        continue;  // Skip the rest of the loop during initialization
      }
    }
    
    // Get the current distance with mutex protection
    if (xSemaphoreTake(distanceMutex, portMAX_DELAY) == pdTRUE) {
      distance = currentDistance;
      xSemaphoreGive(distanceMutex);
    }
    
    // Only process alerts if system is initialized AND sensor has stabilized
    if (systemInitialized && sensorStabilized) {
      // Determine current range based on distance
      // Only consider valid levels (no "normal" state)
      if (distance <= 10) {
        currentRange = 3;  // Close range - Warning
        statusMessage = "Warning";
      } else if (distance <= 20) {
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
        Serial.printf("LCD text locked for %lu seconds\n", LCD_LOCK_TIME_MS / 1000);
        
        // Create appropriate SMS message based on flood level
        switch(currentRange) {
          case 1: // Alert
            createAlertSMS(distance);
            Serial.println("Created Alert SMS for Level 1");
            
            // Reset audio playback finished flag before starting new playback
            if (xSemaphoreTake(audioStatusMutex, portMAX_DELAY) == pdTRUE) {
              audioPlaybackFinished = false;
              xSemaphoreGive(audioStatusMutex);
            }
            
            // Play audio when Level 1 flood is detected
            Serial.println("Level 1 flood detected! Playing LOW-FLOOD-HIGH.mp3");
            static const char* floodAudioFile = "/LOW-FLOOD-HIGH.mp3";
            xTaskCreate(
              audioPlaybackTask,    // Task function
              "AudioTask",          // Task name
              4096,                 // Stack size
              (void*)floodAudioFile, // Task parameter - audio filename
              1,                    // Task priority
              &audioTaskHandle     // Save task handle to check status
            );
            
            // Create a task to wait for audio to finish, then send SMS
            static int floodLevel = 1;
            xTaskCreate(
              smsSendDelayTask,     // Task function
              "SMSDelayTask",       // Task name
              4096,                 // Stack size
              &floodLevel,          // Current flood level
              1,                    // Task priority
              NULL                  // Task handle
            );
            break;
            
          case 2: // Critical
            createCriticalSMS(distance);
            Serial.println("Created Critical SMS for Level 2");
            
            // Reset audio playback finished flag before starting new playback
            if (xSemaphoreTake(audioStatusMutex, portMAX_DELAY) == pdTRUE) {
              audioPlaybackFinished = false;
              xSemaphoreGive(audioStatusMutex);
            }
            
            // Play audio when Level 2 flood is detected
            Serial.println("Level 2 flood detected! Playing MEDIUM-FLOOD-HIGH2.mp3");
            static const char* mediumFloodAudioFile = "/MEDIUM-FLOOD-HIGH2.mp3";
            xTaskCreate(
              audioPlaybackTask,    // Task function
              "AudioTask",          // Task name
              4096,                 // Stack size
              (void*)mediumFloodAudioFile, // Task parameter - audio filename
              1,                    // Task priority
              &audioTaskHandle     // Save task handle to check status
            );
            
            // Create a task to wait for audio to finish, then send SMS
            static int mediumFloodLevel = 2;
            xTaskCreate(
              smsSendDelayTask,     // Task function
              "SMSDelayTask",       // Task name
              4096,                 // Stack size
              &mediumFloodLevel,    // Current flood level
              1,                    // Task priority
              NULL                  // Task handle
            );
            break;
            
          case 3: // Warning
            createWarningSMS(distance);
            Serial.println("Created Warning SMS for Level 3");
            
            // Reset audio playback finished flag before starting new playback
            if (xSemaphoreTake(audioStatusMutex, portMAX_DELAY) == pdTRUE) {
              audioPlaybackFinished = false;
              xSemaphoreGive(audioStatusMutex);
            }
            
            // Play audio when Level 3 flood is detected
            Serial.println("Level 3 flood detected! Playing HIGH-FLOOD-HIGH2.mp3");
            static const char* highFloodAudioFile = "/HIGH-FLOOD-HIGH2.mp3";
            xTaskCreate(
              audioPlaybackTask,    // Task function
              "AudioTask",          // Task name
              4096,                 // Stack size
              (void*)highFloodAudioFile, // Task parameter - audio filename
              1,                    // Task priority
              &audioTaskHandle     // Save task handle to check status
            );
            
            // Create a task to wait for audio to finish, then send SMS
            static int highFloodLevel = 3;
            xTaskCreate(
              smsSendDelayTask,     // Task function
              "SMSDelayTask",       // Task name
              4096,                 // Stack size
              &highFloodLevel,      // Current flood level
              1,                    // Task priority
              NULL                  // Task handle
            );
            break;
        }
        
        // SMS for all flood levels (1, 2, and 3) are now queued in the smsSendDelayTask
        // after respective audio files finish playing and a 5-second delay
        
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
        if (currentDisplayedStatus != "Normal") { // Or if it's different from the standard weather display
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print(cityName); // Standardized display
          lcd.setCursor(0, 1);
          lcd.print(weatherDescription);
          lcd.print(" ");
          lcd.print((int)temperature);
          lcd.print("C");
          
          currentDisplayedStatus = "Normal";
        }
        
        // Update state
        activeState = 0;
        lcdTextLocked = false; // Make sure lock is released
        
        Serial.println("Timeout reached (10 minutes) - All outputs turned off");
      }
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
        float parsed_latitude = loc.substring(0, commaIndex).toFloat();
        float parsed_longitude = loc.substring(commaIndex + 1).toFloat();
        String parsed_city_from_ipinfo = "";
        if (doc.containsKey("city")) {
          parsed_city_from_ipinfo = doc["city"].as<String>();
        }

        // Safely update global weather data variables
        if (xSemaphoreTake(weatherDataMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
          latitude = parsed_latitude;
          longitude = parsed_longitude;
          if (!parsed_city_from_ipinfo.isEmpty()) {
            cityName = parsed_city_from_ipinfo; // Update global cityName
          }
          xSemaphoreGive(weatherDataMutex);
        } else {
          Serial.println("getLocationFromIpInfo: Failed to take weatherDataMutex for update.");
          // success remains true to allow logging of parsed data, but globals might be stale
        }
        
        // For logging, use the values parsed in this specific function call
        String city_for_log = !parsed_city_from_ipinfo.isEmpty() ? parsed_city_from_ipinfo : "N/A";
        String region_for_log = doc.containsKey("region") ? doc["region"].as<String>() : "N/A";
        String country_for_log = doc.containsKey("country") ? doc["country"].as<String>() : "N/A";

        Serial.print("Detected Location: ");
        Serial.print(city_for_log);
        Serial.print(", ");
        Serial.print(region_for_log);
        Serial.print(", ");
        Serial.println(country_for_log);
        Serial.print("Coordinates from ipinfo.io: ");
        Serial.print(parsed_latitude, 6);
        Serial.print(", ");
        Serial.println(parsed_longitude, 6);
        success = true;
      }
    } else if (error) {
      Serial.print("Failed to parse IPInfo response: ");
      Serial.println(error.c_str());
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
        // if (doc.containsKey("name")) {  // Keep cityName from ipinfo.io if available
        //   cityName = doc["name"].as<String>();
        // }

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
  
  prompt += "Write a 200 word message in Tagalog that: 1) Starts with 'Ayon sa pinakabagong update ng PRAF Technology:' ";
  prompt += "2) Includes flood risk assessment 3) Describes current weather 4) Gives a safety tip.";
  prompt += "atlast have a sense of humour that will lighten the resident mood up.";
  
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
      "📱 PRAF TECHNOLOGY UPDATE 📱\n\n"
      "📍 Location: %s\n"
      "🌤️ Weather: %s\n"
      "🌡️ Temperature: %.1f°C\n"
      "🌡️ Feels like: %.1f°C\n"
      "💧 Humidity: %.0f%%\n\n"
      "🤖 AI Weather Update:\n"
      "%s\n"
      "🤍 Stay safe and informed!\n\n"
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
      "🟡 FLOOD ALERT - LEVEL 1 🟡\n\n"
      "📍 Location: %s\n"
      "🔍 Current water level: %.1f cm\n"
      "📢 Status: ALERT - Initial flooding detected\n\n"
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
      "🟠 FLOOD CRITICAL - LEVEL 2 🟠\n\n"
      "📍 Location: %s\n"
      "🔍 Current water level: %.1f cm\n"
      "📢 Status: CRITICAL - Significant flooding\n\n"
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
      "🔴 FLOOD WARNING - LEVEL 3 🔴\n\n"
      "📍 Location: %s\n"
      "🔍 Current water level: %.1f cm\n"
      "📢 Status: WARNING - Severe flooding\n\n"
      "🚨 EMERGENCY ACTIONS REQUIRED:\n"
      "- EVACUATE immediately if instructed\n"
      "- Move to higher ground NOW\n"
      "- Follow emergency routes\n"
      "- Do NOT attempt to cross floodwaters\n"
      "- Call emergency services if trapped\n\n"
      "Stay safe.\n\n"
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
  String endpoint = String(supabaseUrl) + "/rest/v1/" + tableName + "?select=phone_number,name";
  
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
            String number = entry["phone_number"].as<String>();
            phoneNumbers[numPhoneNumbers] = number;
            String name = entry["name"].as<String>();
            phoneNames[numPhoneNumbers] = name;
            
            Serial.print("Number ");
            Serial.print(numPhoneNumbers + 1);
            Serial.print(": ");
            Serial.print(number);
            Serial.print(" (");
            Serial.print(name);
            Serial.println(")");
            
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
void sendHttpSMS(const char* from, const char* to, const char* body, const char* recipientName = NULL) {
  Serial.print("Sending SMS to: ");
  Serial.print(to);
  if (recipientName) {
    Serial.print(" (");
    Serial.print(recipientName);
    Serial.print(")");
  }
  Serial.println();
  
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
  
  // Create a personalized message if recipient name is provided
  String personalizedBody = body;
  if (recipientName && strlen(recipientName) > 0) {
    // Check for different "From:" line variations
    int fromIndex = personalizedBody.indexOf("From: PRAF Technology");
    if (fromIndex < 0) {
      // Try alternative "From:" line
      fromIndex = personalizedBody.indexOf("From: PRAF Technology Flood Monitoring System");
    }
    
    if (fromIndex > 0) {
      // Find the end of the string or next newline after "From:"
      int lineEndIndex = personalizedBody.length();
      int nextNewline = personalizedBody.indexOf('\n', fromIndex);
      if (nextNewline > 0) {
        lineEndIndex = nextNewline;
      }
      
      // Add "To:" line on a new line after "From:"
      personalizedBody = personalizedBody.substring(0, lineEndIndex) + 
                        "\nTo: " + String(recipientName) + 
                        personalizedBody.substring(lineEndIndex);
    } else {
      // If no "From:" line found, append to end
      personalizedBody += "\nTo: " + String(recipientName);
    }
    
    Serial.println("Personalized message created:");
    Serial.println(personalizedBody);
  }
  
  // Create JSON payload
  DynamicJsonDocument doc(1024);
  doc["content"] = personalizedBody;
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
  
  // --- INITIAL DATA FETCH AT STARTUP ---
  Serial.println("WeatherTask: Initial data fetch sequence started.");
  bool initialLocationSuccess = false;
  int retries = 0;
  while (!initialLocationSuccess && retries < 3) {
    if (getLocationFromIpInfo()) {
      initialLocationSuccess = true;
      Serial.println("WeatherTask: Initial location fetched successfully.");
    } else {
      Serial.println("WeatherTask: Initial location fetch attempt failed. Retrying...");
      vTaskDelay(pdMS_TO_TICKS(2000));
      retries++;
    }
  }
  
  if (!initialLocationSuccess) {
    Serial.println("WeatherTask: Could not get initial location from IP. Using fallback coordinates.");
    // Safely take mutex to update coordinates
    if (xSemaphoreTake(weatherDataMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
      latitude = fallback_latitude;
      longitude = fallback_longitude;
      // cityName might remain default or be updated by a failed getLocationFromIpInfo attempt
      xSemaphoreGive(weatherDataMutex);
    }
  }

  // Fetch initial weather and AI suggestion regardless of location success (will use current lat/lon)
  retries = 0;
  bool initialWeatherSuccess = false;
  while(!initialWeatherSuccess && retries < 3) {
    if (getWeather()) {
      Serial.println("WeatherTask: Initial weather data fetched successfully.");
      initialWeatherSuccess = true;
      // Get AI suggestion with a delay to avoid stack issues
      vTaskDelay(pdMS_TO_TICKS(1000)); // Small delay before AI call
      if (getAISuggestion()) {
        Serial.println("WeatherTask: Initial AI suggestion fetched successfully.");
      } else {
        Serial.println("WeatherTask: Initial AI suggestion fetch failed. Default AI message will be used.");
      }
    } else {
      Serial.println("WeatherTask: Initial weather fetch attempt failed. Retrying...");
      vTaskDelay(pdMS_TO_TICKS(2000));
      retries++;
    }
  }
   if (!initialWeatherSuccess) {
    Serial.println("WeatherTask: Could not fetch initial weather data. Default weather values will be used.");
  }
  
  updateSmsBody(); // Update SMS body with initial (or default) data
  weatherInitialized = true; // Signal that initial setup is complete
  Serial.println("WeatherTask: Initial data fetch sequence complete.");

  // --- PERIODIC UPDATE LOOP ---
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(900000)); // Wait 15 minutes for the next update cycle

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WeatherTask: Periodic update cycle started.");

      // Store current location data before attempting to update
      float previousLatitude, previousLongitude;
      String previousCityName;
      if (xSemaphoreTake(weatherDataMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        previousLatitude = latitude;
        previousLongitude = longitude;
        previousCityName = cityName;
        xSemaphoreGive(weatherDataMutex);
      } else {
        Serial.println("WeatherTask: Could not get mutex for previous location data. Skipping update cycle.");
        continue;
      }

      bool newLocationFetched = getLocationFromIpInfo();
      
      if (newLocationFetched) {
        Serial.println("WeatherTask: Successfully fetched new location data for periodic update.");
        bool locationHasActuallyChanged = false;
        // Check if location actually changed
        if (xSemaphoreTake(weatherDataMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
          if (latitude != previousLatitude || longitude != previousLongitude || cityName != previousCityName) {
            locationHasActuallyChanged = true;
            Serial.printf("WeatherTask: Location changed. Old: %s (%.6f, %.6f), New: %s (%.6f, %.6f)\n",
                          previousCityName.c_str(), previousLatitude, previousLongitude,
                          cityName.c_str(), latitude, longitude);
          } else {
            Serial.println("WeatherTask: Location data refreshed, but coordinates and city name are unchanged.");
          }
          xSemaphoreGive(weatherDataMutex);
        } else {
            Serial.println("WeatherTask: Could not get mutex to check if location changed. Assuming it did not.");
        }

        if (locationHasActuallyChanged) {
          Serial.println("WeatherTask: Location has changed, proceeding to update weather and AI suggestion.");
          if (getWeather()) {
            Serial.println("WeatherTask: Weather data updated successfully after location change.");
            vTaskDelay(pdMS_TO_TICKS(1000)); // Small delay
            if (getAISuggestion()) {
              Serial.println("WeatherTask: AI suggestion updated successfully after location change.");
            } else {
              Serial.println("WeatherTask: AI suggestion update failed after location change.");
            }
            updateSmsBody();
            Serial.println("WeatherTask: SMS body updated with new data after location change.");
          } else {
            Serial.println("WeatherTask: Weather update failed after location change. SMS body not updated with new weather.");
          }
        } else {
          Serial.println("WeatherTask: Location unchanged, skipping weather/AI update.");
        }
      } else {
        Serial.println("WeatherTask: Periodic location update failed. Weather/AI not updated.");
      }
    } else {
      Serial.println("WeatherTask: WiFi disconnected. Cannot perform periodic update.");
    }
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
      // If AI button is pressed (LOW)
      if (ai == LOW) {
        Serial.println("AI Button pressed! Playing AI notification sound...");

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("AI Assistant"); // New LCD message
        lcd.setCursor(0, 1);
        lcd.print("Processing...");  // New LCD message

        // 1. Play AI-NOTIF.mp3
        if (xSemaphoreTake(audioStatusMutex, portMAX_DELAY) == pdTRUE) {
          audioPlaybackFinished = false;
          xSemaphoreGive(audioStatusMutex);
        }
        static const char* aiNotifFile = "/AI-NOTIF.mp3";
        xTaskCreate(
          audioPlaybackTask,    // Task function
          "AINotifAudioTask",   // Task name
          4096,                 // Stack size
          (void*)aiNotifFile,   // Task parameter
          1,                    // Task priority
          NULL                  // Task handle
        );

        // 2. Create the LED animation task
        xTaskCreate(
          ledAnimationTask,  // Task function
          "LEDTask",         // Task name
          2000,              // Stack size
          NULL,              // Task parameters
          1,                 // Priority
          &ledTaskHandle     // Task handle
        );
        
        // 3. Get the weather message
        String* weatherTextToSpeak = NULL;
        if (xSemaphoreTake(weatherDataMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
          weatherTextToSpeak = new String(aiWeatherMessage);
          xSemaphoreGive(weatherDataMutex);
        } else {
          weatherTextToSpeak = new String("Hindi makuha ang weather update. Subukan ulit.");
          Serial.println("Failed to get weather message for TTS, using default");
        }
        
        // 4. Create task to wait for AI-NOTIF.mp3 then play TTS
        // waitForAudioThenTTS will manage audioPlaybackFinished for the TTS part
        xTaskCreate(
          waitForAudioThenTTS, // Task function
          "WaitThenTTSTask",   // Task name
          8192,                // Stack size
          weatherTextToSpeak,  // Pass weather message as parameter
          1,                   // Task priority
          NULL                 // Task handle
        );
        
        // Wait for button release to prevent multiple triggers
        while (digitalRead(BTTN_AI) == LOW) {
          vTaskDelay(pdMS_TO_TICKS(10));
        }
      }

      // If SMS button is pressed (LOW)
      if (reading == LOW) { // Check 'reading' for SMS button, not 'ai'
        Serial.println("SMS Button pressed! Playing SMS sent sound...");

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("SMS Queued");     // New LCD message
        lcd.setCursor(0, 1);
        lcd.print("Please Wait..."); // New LCD message

        // 2. Create the LED animation task (as added by user)
        xTaskCreate(
          ledAnimationTask,  // Task function
          "LEDTask",         // Task name
          2000,              // Stack size
          NULL,              // Task parameters
          1,                 // Priority
          &ledTaskHandle     // Task handle
        );
        

        // 1. Play SMS-SENT-VOICE.mp3
        if (xSemaphoreTake(audioStatusMutex, portMAX_DELAY) == pdTRUE) {
          audioPlaybackFinished = false;
          xSemaphoreGive(audioStatusMutex);
        }
        static const char* smsSentFile = "/SMS-SENT-VOICE.mp3";
        xTaskCreate(
          audioPlaybackTask,    // Task function
          "SMSSentAudioTask",   // Task name
          4096,                 // Stack size
          (void*)smsSentFile,   // Task parameter
          1,                    // Task priority
          NULL                  // Task handle
        );

        // 3. Queue the SMS
        Serial.println("Button pressed! Queueing SMS..."); // Original log
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
    // Check if audio is currently playing
    bool isAudioPlaying = false;
    if (xSemaphoreTake(audioStatusMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      isAudioPlaying = !audioPlaybackFinished;
      xSemaphoreGive(audioStatusMutex);
    }
    
    // Only fetch phone numbers if audio is not playing
    if (WiFi.status() == WL_CONNECTED && !isAudioPlaying) {
      Serial.println("Fetching phone numbers...");
      fetchPhoneNumbers();
    } else if (isAudioPlaying) {
      Serial.println("Audio is playing - skipping phone number fetch");
    }
    
    // Check every 10 seconds for new phone numbers
    vTaskDelay(pdMS_TO_TICKS(10000));
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
              
              sendHttpSMS(smsFrom, phoneNumbers[i].c_str(), smsBody, phoneNames[i].c_str());
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
      }   
    }
  }
}

// Audio playback task
void audioPlaybackTask(void *parameter) {
  Serial.println("Starting audio playback task");
  
  // Determine which file to play based on task parameter
  const char* filename = (const char*)parameter;
  if (filename == NULL) {
    filename = "/DEVICE-START-VOICE.mp3";  // Default file if no parameter
  }
  
  Serial.print("Opening audio file: ");
  Serial.println(filename);
  
  // Open MP3 file from SD card
  mp3File = SD.open(filename);
  if (!mp3File) {
    Serial.println("Failed to open MP3 file!");
    vTaskDelete(NULL);
    return;
  }
  
  // Configure I2S
  auto i2sConfig = i2sStream.defaultConfig(TX_MODE);
  i2sConfig.pin_bck = I2S_BCLK;
  i2sConfig.pin_ws = I2S_LRC;
  i2sConfig.pin_data = I2S_DOUT;
  i2sConfig.buffer_size = 4096;  // Reduced buffer size
  i2sConfig.buffer_count = 4;    // Reduced buffer count
  i2sConfig.channels        = 2;
  i2sConfig.channel_format  = I2SChannelSelect::Stereo;
  i2sStream.begin(i2sConfig);
  
  // Initialize decoder
  decoder.begin();
  
  // Set up copier
  copier.begin(decoder, mp3File);
  
  Serial.println("Audio playback started");
  
  // Play the audio file
  while (copier.copy()) {
    // Allow other tasks to run
    vTaskDelay(1);
  }
  
  // Clean up
  decoder.end();
  i2sStream.end();
  mp3File.close();
  
  Serial.println("Audio playback completed");
  
  // Set flag that audio playback is finished
  if (xSemaphoreTake(audioStatusMutex, portMAX_DELAY) == pdTRUE) {
    audioPlaybackFinished = true;
    xSemaphoreGive(audioStatusMutex);
  }
  
  // Delete this task when done
  vTaskDelete(NULL);
}

// Task to wait for audio playback to finish before sending SMS
void smsSendDelayTask(void *parameter) {
  int floodLevel = *((int*)parameter);
  Serial.printf("Starting SMS delay task for flood level %d\n", floodLevel);
  
  // Wait for audio playback to finish
  bool isAudioFinished = false;
  while (!isAudioFinished) {
    if (xSemaphoreTake(audioStatusMutex, portMAX_DELAY) == pdTRUE) {
      isAudioFinished = audioPlaybackFinished;
      xSemaphoreGive(audioStatusMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // Check every 100ms
  }
  
  // Different delay times based on flood level
  int delaySeconds = 2; // Default for levels 1 and 2
  
  // Use 8 seconds delay for high flood level (level 3)
  if (floodLevel == 3) {
    delaySeconds = 4;
  }
  
  Serial.printf("Audio playback finished. Waiting additional %d seconds before sending SMS...\n", delaySeconds);
  
  // Wait the specified delay time
  vTaskDelay(pdMS_TO_TICKS(delaySeconds * 1000));
  
  Serial.printf("Delay completed. Queuing SMS for flood level %d\n", floodLevel);
  
  // Queue SMS with appropriate signal value
  int signalValue = 10 + floodLevel; // 11=Alert, 12=Critical, 13=Warning
  if (xQueueSend(smsQueue, &signalValue, 0) == pdTRUE) {
    Serial.printf("Queued SMS for flood level %d\n", floodLevel);
  } else {
    Serial.println("Failed to queue SMS - queue might be full");
  }
  
  // Delete this task
  vTaskDelete(NULL);
}

// URL encode a string (for TTS API)
String urlEncode(const String& text) {
  String encoded = "";
  char c;
  char code0;
  char code1;
  
  for (int i = 0; i < text.length(); i++) {
    c = text.charAt(i);
    if (c == ' ') {
      encoded += '+';
    } else if (isalnum(c)) {
      encoded += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) {
        code0 = c - 10 + 'A';
      }
      encoded += '%';
      encoded += code0;
      encoded += code1;
    }
  }
  return encoded;
}

// Build a TTS URL with proper encoding
String makeTTSUrl(const String& chunk, const char* lang, const char* speed) {
  String url = queryTemplate;
  url.replace("%1", lang);
  url.replace("%2", speed);
  String encoded = urlEncode(chunk);
  url.replace("%3", encoded);
  return url;
}

// Split text into chunks for TTS (Google TTS has a 200 character limit)
void splitText(const String& text, size_t maxLen, std::vector<String>& outChunks) {
  size_t start = 0, len = text.length();
  
  outChunks.clear();
  
  while (start < len) {
    // Determine end idx
    size_t end = start + maxLen;
    if (end >= len) {
      end = len;
    } else {
      // backtrack to last space
      size_t lastSpace = text.lastIndexOf(' ', end);
      if (lastSpace > start) end = lastSpace;
    }
    outChunks.push_back(text.substring(start, end));
    // skip any spaces at beginning of next chunk
    start = end;
    while (start < len && text.charAt(start) == ' ') start++;
  }
}

// TTS playback task - plays text using Google TTS API
void ttsPlaybackTask(void *parameter) {
  Serial.println("Starting TTS playback task");
  
  // Reset audio playback flag
  if (xSemaphoreTake(audioStatusMutex, portMAX_DELAY) == pdTRUE) {
    audioPlaybackFinished = false;
    xSemaphoreGive(audioStatusMutex);
  }
  
  // Get text from parameter
  String* textToSpeak = (String*)parameter;
  if (textToSpeak == NULL || textToSpeak->length() == 0) {
    Serial.println("No text to speak!");
    
    // Set flag that audio playback is finished (even though it didn't start)
    if (xSemaphoreTake(audioStatusMutex, portMAX_DELAY) == pdTRUE) {
      audioPlaybackFinished = true;
      xSemaphoreGive(audioStatusMutex);
    }
    
    vTaskDelete(NULL);
    return;
  }
  
  Serial.print("Text to speak: ");
  Serial.println(*textToSpeak);
  
  // Configure I2S with proper audio settings
  auto i2sConfig = i2sStream.defaultConfig(TX_MODE);
  i2sConfig.pin_bck = I2S_BCLK;
  i2sConfig.pin_ws = I2S_LRC;
  i2sConfig.pin_data = I2S_DOUT;
  i2sConfig.bits_per_sample = 16;      // Standard for audio
  i2sConfig.sample_rate = 44100;       // Standard for MP3
  i2sConfig.buffer_size = 4096;        // Reduced buffer size
  i2sConfig.buffer_count = 4;          // Reduced buffer count
  i2sConfig.channels = 2;
  i2sConfig.channel_format = I2SChannelSelect::Stereo;
  i2sStream.begin(i2sConfig);
  
  // Initialize decoder
  ttsDecoder.begin();
  
  // Split into ≤200‐char chunks (Google TTS limit)
  std::vector<String> chunks;
  splitText(*textToSpeak, 200, chunks);
  Serial.printf("Text divided into %u chunk(s)\n", chunks.size());
  
  // Stream each chunk in order
  for (size_t i = 0; i < chunks.size(); ++i) {
    String urlStr = makeTTSUrl(chunks[i], "tl", "1");
    Serial.printf("Chunk %u URL:\n%s\n", i + 1, urlStr.c_str());

    // Start streaming this chunk
    url.begin(urlStr.c_str(), "audio/mp3");
    
    // Wait for connection and buffer to fill
    if (i == 0) {
      Serial.printf("Buffering initial chunk %u/%u (will wait 500ms)...\n", i + 1, chunks.size());
      vTaskDelay(pdMS_TO_TICKS(500));  // Allow time for initial buffer to fill
    } else {
      Serial.printf("Buffering subsequent chunk %u/%u (will wait 200ms)...\n", i + 1, chunks.size());
      // Add a smaller, consistent delay for subsequent chunks to allow some initial buffering
      vTaskDelay(pdMS_TO_TICKS(200)); 
    }
    
    Serial.printf("Playing chunk %u/%u...\n", i + 1, chunks.size()); // Log placed before copy loop
    
    size_t current_chunk_bytes_read = 0; // Switched to size_t
    int empty_reads_count = 0;
    const int MAX_EMPTY_READS_STALL = 150;  // Increased timeout: 150 cycles * 20ms/cycle = 3 seconds
    const int TTS_COPY_LOOP_DELAY_MS = 20; // Adjusted delay

    while (true) {
        size_t bytes_copied_this_step = ttsCopier.copy(); // Switched to size_t

        if (bytes_copied_this_step > 0) {
            current_chunk_bytes_read += bytes_copied_this_step;
            empty_reads_count = 0;  // Reset counter as we received data
        } else { // bytes_copied_this_step == 0 (since size_t is unsigned)
            empty_reads_count++;
            if (empty_reads_count > MAX_EMPTY_READS_STALL) {
                Serial.printf("Chunk %u: Timeout after %d empty reads. Assuming end of chunk or network stall.\n", i + 1, empty_reads_count);
                break; 
            }
        }
        vTaskDelay(pdMS_TO_TICKS(TTS_COPY_LOOP_DELAY_MS));
    }
    
    Serial.printf("Chunk %u done. Read %zu bytes\n", i + 1, current_chunk_bytes_read); // Use %zu for size_t
    
    // Close URL stream for this chunk
    url.end();
    
    // Small delay between chunks
    vTaskDelay(pdMS_TO_TICKS(50)); // Reduced delay from 100ms to 50ms
  }
  
  Serial.println("TTS playback completed");
  
  // Clean up
  ttsDecoder.end();
  i2sStream.end();
  
  // Set flag that audio playback is finished
  if (xSemaphoreTake(audioStatusMutex, portMAX_DELAY) == pdTRUE) {
    audioPlaybackFinished = true;
    xSemaphoreGive(audioStatusMutex);
  }
  
  // Free the parameter memory
  delete textToSpeak;
  
  // Delete this task
  vTaskDelete(NULL);
}

// Task to wait for audio playback to finish before playing TTS
void waitForAudioThenTTS(void *parameter) {
  String* textToSpeak = (String*)parameter;
  
  if (textToSpeak == NULL) {
    Serial.println("No TTS text provided, exiting task");
    vTaskDelete(NULL);
    return;
  }
  
  Serial.println("Waiting for audio file to finish before playing TTS...");
  
  // Wait for audio playback to finish
  bool isAudioFinished = false;
  while (!isAudioFinished) {
    if (xSemaphoreTake(audioStatusMutex, portMAX_DELAY) == pdTRUE) {
      isAudioFinished = audioPlaybackFinished;
      xSemaphoreGive(audioStatusMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // Check every 100ms
  }
  
  Serial.println("Audio file finished, now playing TTS message");
  
  // Reset audio playback flag for TTS playback
  if (xSemaphoreTake(audioStatusMutex, portMAX_DELAY) == pdTRUE) {
    audioPlaybackFinished = false;
    xSemaphoreGive(audioStatusMutex);
  }
  
  // Create TTS playback task
  xTaskCreate(
    ttsPlaybackTask,     // Task function
    "TTSSeqTask",        // Task name
    8192,                // Stack size
    textToSpeak,         // Pass TTS text as parameter (memory will be freed by ttsPlaybackTask)
    1,                   // Task priority
    NULL                 // Task handle not needed
  );
  
  // This task can now end as ttsPlaybackTask will handle the TTS playback
  vTaskDelete(NULL);
}

void setup() {
  // Set microSD Card CS pin  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  // Initialize serial communication
  Serial.begin(115200);
  
  // Record startup time
  startupTime = millis();
  systemInitialized = false;  // Ensure system starts uninitialized
  sensorStabilized = false;   // Ensure sensor starts unstabilized
  stabilizationReadings = 0;  // Reset stabilization counter

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

  // Configure LED pins first and make sure they're off
  pinMode(LED_ONE, OUTPUT);
  pinMode(LED_TWO, OUTPUT);
  pinMode(LED_THREE, OUTPUT);
 
  // Explicitly turn off all LEDs at startup
  digitalWrite(LED_ONE, LOW);
  digitalWrite(LED_TWO, LOW);
  digitalWrite(LED_THREE, LOW);
  pinMode(AI_LED_ONE, OUTPUT);
  pinMode(AI_LED_TWO, OUTPUT);
  pinMode(AI_LED_THREE, OUTPUT);

  Serial.println("\nESP32 Button SMS Sender with FreeRTOS, Supabase and Weather Integration");

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

  // Configure button pins after LEDs
  pinMode(BTTN_SMS, INPUT_PULLUP);
  pinMode(BTTN_AI, INPUT_PULLUP);
  
  // Create mutex for shared data protection
  distanceMutex = xSemaphoreCreateMutex();
  
  // Initialize LCD-related variables
  lastLCDUpdateTime = 0;
  currentDisplayedStatus = "Normal";
  lcdTextLocked = false;
  
  // Create mutexes
  phoneNumbersMutex = xSemaphoreCreateMutex();
  weatherDataMutex = xSemaphoreCreateMutex();
  audioStatusMutex = xSemaphoreCreateMutex();
  
  // Create queue for button events
  smsQueue = xQueueCreate(5, sizeof(int));

  // Initialize SPI for SD card
  spiSD.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (!SD.begin(SD_CS, spiSD)) {
    Serial.println("❌ SD init failed");
    while (1) delay(100);
  }
  Serial.println("✅ SD initialized");

  Serial.println("SD card initialized.");

  // List files on SD card
  // listDir(SD, "/", 0);

  // Play startup sound
  Serial.println("Playing startup sound: /DEVICE-START-VOICE.mp3");
  static const char* startupAudioFile = "/DEVICE-START-VOICE.mp3";
  xTaskCreate(
    audioPlaybackTask,    // Task function
    "StartupAudioTask",   // Task name
    4096,                 // Stack size
    (void*)startupAudioFile, // Task parameter - audio filename
    1,                    // Task priority
    NULL                  // Task handle (not needed for this one-shot task)
  );

  // Audio will play only when AI button is pressed
  // Removed automatic audio playback task creation

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
    10000,           // Increased stack size from 4096 to 8192
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

  // Initialize TTS URL stream with WiFi credentials
  url.begin(ssid, password);

  Serial.println("ESP32 HC-SR04 Distance Sensor with FreeRTOS Started");
}

void loop() {
  // Empty loop as tasks are handled by FreeRTOS
  vTaskDelay(pdMS_TO_TICKS(1000));
}
