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

// WiFi credentials
const char* ssid = "TK-gacura";
const char* password = "gisaniel924";

// HttpSMS API key
const char* httpSmsApiKey = "MNJmgF7kRvUrTfj4fqDUbrzwoVFpMToWdTbiUx3sQ6jreYnbnu7bym-rQG3kB8_U";

// Supabase configuration
const char* supabaseUrl = "https://jursmglsfqaqrxvirtiw.supabase.co";
const char* supabaseKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Imp1cnNtZ2xzZnFhcXJ4dmlydGl3Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NDQ3ODkxOTEsImV4cCI6MjA2MDM2NTE5MX0.ajGbf9fLrYAA0KXzYhGFCTju-d4h-iTYTeU5WfITj3k";
const char* tableName = "resident_number";

// Weather API configuration
const char* weatherApiKey = "7970309436bc52d518c7e71e314b8053";

// Fallback coordinates for STI College Fairview
float fallback_latitude = 14.7160;
float fallback_longitude = 121.0615;

// Global variables for weather data
float latitude = 0.0;
float longitude = 0.0;
String weatherDescription = "unknown";
float temperature = 0.0;
float feelsLike = 0.0;
float humidity = 0.0;
String cityName = "Caloocan"; // Default city

// SMS configuration
const char* smsFrom = "+639649687066"; // Your sender number or name
String phoneNumbers[10]; // Array to store up to 10 phone numbers
int numPhoneNumbers = 0;
char smsBody[1024]; // Buffer for dynamic SMS content

// Button configuration
#define BTTN_SMS 2                  // Button connected to GPIO pin 2
#define DEBOUNCE_DELAY 300          // Debounce time in milliseconds

// FreeRTOS handles
TaskHandle_t buttonTaskHandle = NULL;
TaskHandle_t wifiTaskHandle = NULL;
TaskHandle_t databaseTaskHandle = NULL;
TaskHandle_t weatherTaskHandle = NULL;
QueueHandle_t smsQueue = NULL;
SemaphoreHandle_t phoneNumbersMutex = NULL;
SemaphoreHandle_t weatherDataMutex = NULL;

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

        if (doc.containsKey("city")) {
          cityName = doc["city"].as<String>();
        }

        Serial.print("Detected Location: ");
        Serial.println(cityName);
        Serial.print("Coordinates: ");
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

        weatherDescription = "unknown";
        if (doc.containsKey("weather") && doc["weather"][0].containsKey("description")) {
          weatherDescription = doc["weather"][0]["description"].as<String>();
        }

        if (doc.containsKey("main")) {
          temperature = doc["main"]["temp"].as<float>();
          feelsLike = doc["main"]["feels_like"].as<float>();
          humidity = doc["main"]["humidity"].as<float>();
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
      Serial.println("Error parsing weather data");
    }
  } else {
    Serial.println("Failed to connect to OpenWeather API, HTTP code: " + String(httpCode));
  }
  http.end();
  
  return success;
}

// Function to update SMS body with current weather data
void updateSmsBody() {
  if (xSemaphoreTake(weatherDataMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    snprintf(smsBody, sizeof(smsBody),
      "🚨 FLOOD ALERT! 🚨\n\n"
      "📱 System Check:\n\n"
      "📍 Location: %s\n"
      "🌤️ Weather: %s\n"
      "🌡️ Temperature: %.1f°C\n"
      "🌡️ Feels like: %.1f°C\n"
      "💧 Humidity: %.0f%%\n\n"
      "🤖 AI Weather Update:\n"
      "Ayon sa pinakabagong update ng PRAF Technology: Sa kasalukuyan, walang banta ng baha sa %s. "
      "Ang panahon ay %s, na may temperaturang %.2f°C, ngunit dahil sa %.0f%% na halumigmig (humidity), "
      "mas ramdam and init na umaabot sa %.2f°C. Pinapayuhan ang lahat na magsuot ng magagaan at preskong damit "
      "at uminom ng maraming tubig upang makaiwas sa epekto ng matinding init.\n\n"
      "From: PRAF Technology",
      cityName.c_str(), weatherDescription.c_str(), temperature, feelsLike, humidity,
      cityName.c_str(), weatherDescription.c_str(), temperature, humidity, feelsLike
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
  while (client.connected() || client.available()) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      Serial.println(line);
    }
  }
  client.stop();
  Serial.println("SMS Connection closed");
}

// Weather data fetch task
void weatherTask(void *pvParameters) {
  // First get location
  if (!getLocationFromIpInfo()) {
    // Use fallback coordinates if unable to get location
    latitude = fallback_latitude;
    longitude = fallback_longitude;
    Serial.println("Using fallback coordinates");
  }
  
  // Get initial weather data
  getWeather();
  updateSmsBody();
  
  while (1) {
    if (WiFi.status() == WL_CONNECTED) {
      if (getWeather()) {
        updateSmsBody();
        Serial.println("Weather data and SMS body updated");
      }
    }
    // Update weather every 15 minutes (900000 ms)
    vTaskDelay(pdMS_TO_TICKS(900000));
  }
}

// Button monitoring task
void buttonTask(void *pvParameters) {
  int lastButtonState = HIGH;
  int buttonState = HIGH;
  unsigned long lastDebounceTime = 0;
  
  while (1) {
    int reading = digitalRead(BTTN_SMS);
    
    if (reading != lastButtonState) {
      lastDebounceTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
    }
    
    if ((xTaskGetTickCount() * portTICK_PERIOD_MS - lastDebounceTime) > DEBOUNCE_DELAY) {
      if (reading != buttonState) {
        buttonState = reading;
        
        if (buttonState == LOW) {
          Serial.println("Button pressed! Queueing SMS...");
          // Send a message to the queue
          int signalValue = 1;
          xQueueSend(smsQueue, &signalValue, 0);
        }
      }
    }
    
    lastButtonState = reading;
    vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to prevent task starvation
  }
}

// Database task - periodically fetches phone numbers
void databaseTask(void *pvParameters) {
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
  int signalValue;
  
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
              sendHttpSMS(smsFrom, phoneNumbers[i].c_str(), smsBody);
              // Small delay between sending messages
              vTaskDelay(pdMS_TO_TICKS(500));
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

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nESP32 Button SMS Sender with FreeRTOS, Supabase and Weather Integration");
  
  // Set button pin as input with internal pull-up resistor
  pinMode(BTTN_SMS, INPUT_PULLUP);
  
  // Create mutexes
  phoneNumbersMutex = xSemaphoreCreateMutex();
  weatherDataMutex = xSemaphoreCreateMutex();
  
  // Create queue for button events
  smsQueue = xQueueCreate(5, sizeof(int));
  
  // Create tasks
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
    4096,           // Stack size
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
}

void loop() {
  // Empty loop as tasks are handled by FreeRTOS
  vTaskDelay(pdMS_TO_TICKS(1000));
}
