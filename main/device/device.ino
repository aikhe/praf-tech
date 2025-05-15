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

// CHECKPOINT

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

// Add new variables for connectivity checking
unsigned long lastConnectivityCheck = 0;
const unsigned long connectivityCheckInterval = 60000;  // Check connectivity every minute

// Add this helper function to troubleshoot HTTP connection issues
bool troubleshootHTTPConnection(int errorCode, const char* endpoint) {
  if (errorCode == -1) { // Connection refused
    Serial.println("⚠️ Connection refused - troubleshooting network...");
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Network Error");
    lcd.setCursor(0, 1);
    lcd.print("Troubleshooting...");
    
    // First check basic WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("📡 WiFi disconnected - reconnecting...");
      
      // Full WiFi reset sequence
      WiFi.disconnect(true);  // Disconnect with clearing settings
      delay(1000);
      WiFi.mode(WIFI_STA); // Set station mode
      delay(1000);
      WiFi.begin(SSID, PASSWORD);
      
      // Wait up to 10 seconds for reconnection
      int timeout = 10;
      while (WiFi.status() != WL_CONNECTED && timeout > 0) {
        delay(1000);
        Serial.print(".");
        timeout--;
      }
      
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n❌ WiFi reconnection failed");
        return false;
      }
      
      Serial.println("\n✅ WiFi reconnected");
    }
    
    // Test basic HTTP connectivity first (no SSL)
    Serial.println("🔍 Testing basic HTTP connectivity...");
    HTTPClient httpTest;
    httpTest.setTimeout(3000); // Short timeout for test
    httpTest.begin("http://www.google.com"); // Using HTTP not HTTPS
    int httpCode = httpTest.GET();
    httpTest.end();
    
    if (httpCode < 0) {
      Serial.print("❌ Basic HTTP test failed, code: ");
      Serial.println(httpCode);
    } else {
      Serial.println("✅ Basic HTTP connectivity test passed");
    }
    
    // Test HTTPS with WiFiClientSecure
    Serial.println("🔍 Testing HTTPS connectivity...");
    WiFiClientSecure secureClient;
    // Skip certificate validation for troubleshooting
    secureClient.setInsecure();
    
    // Try to connect to a known HTTPS endpoint
    if (secureClient.connect("www.google.com", 443)) {
      Serial.println("✅ HTTPS connection successful");
      secureClient.stop();
      
      // Now try to connect to the specific endpoint
      Serial.print("🔍 Testing HTTPS connection to: ");
      Serial.println(endpoint);
      
      // Extract host from endpoint (remove https:// if present)
      String host = endpoint;
      if (host.startsWith("https://")) {
        host = host.substring(8);
      }
      // Extract just the domain part if there's a path
      int pathPos = host.indexOf('/');
      if (pathPos > 0) {
        host = host.substring(0, pathPos);
      }
      
      if (secureClient.connect(host.c_str(), 443)) {
        Serial.print("✅ Successfully connected to ");
        Serial.println(host);
        secureClient.stop();
        
        // Update DNS settings since secure connection works
        Serial.println("🔄 Setting Google DNS servers to improve reliability...");
        IPAddress primaryDNS(8, 8, 8, 8);
        IPAddress secondaryDNS(8, 8, 4, 4);
        WiFi.disconnect();
        delay(1000);
        if (!WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), primaryDNS, secondaryDNS)) {
          Serial.println("❌ DNS configuration failed but connectivity works");
        }
        WiFi.begin(SSID, PASSWORD);
        int timeout = 10;
        while (WiFi.status() != WL_CONNECTED && timeout > 0) {
          delay(1000);
          Serial.print(".");
          timeout--;
        }
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Connection fixed");
        delay(1000);
        
        return true;
      } else {
        Serial.print("❌ Failed to connect to ");
        Serial.println(host);
      }
    } else {
      Serial.println("❌ HTTPS connection failed");
    }
    
    // If we're here, basic SSL tests failed, try changing DNS servers
    Serial.println("🔄 Setting Google DNS servers (8.8.8.8, 8.8.4.4)...");
    IPAddress primaryDNS(8, 8, 8, 8);
    IPAddress secondaryDNS(8, 8, 4, 4);
    
    WiFi.disconnect();
    delay(1000);
    
    // Attempt to reconnect with custom DNS
    if (!WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), primaryDNS, secondaryDNS)) {
      Serial.println("❌ DNS configuration failed");
    }
    
    WiFi.begin(SSID, PASSWORD);
    
    // Wait for connection
    int timeout = 10; // Declare a new timeout variable in this scope
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
      delay(1000);
      Serial.print(".");
      timeout--;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("\n❌ WiFi reconnection with custom DNS failed");
      return false;
    }
    
    Serial.println("\n✅ WiFi reconnected with custom DNS");
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Network reset");
    lcd.setCursor(0, 1);
    lcd.print("Testing again...");
    
    return true; // Successfully troubleshot connection issues
  }
  
  // For other error codes, we don't need to troubleshoot
  return false;
}

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
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");
  
  Serial.print("Connecting to WiFi...");
  int connectTimeout = 0;
  while (WiFi.status() != WL_CONNECTED && connectTimeout < 20) {
    delay(1000);
    Serial.print(".");
    lcd.setCursor(connectTimeout % 16, 1);
    lcd.print(".");
    connectTimeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi\n");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    delay(1000);
    
    // Verify internet connectivity
    if (!checkInternetConnection()) {
      Serial.println("WiFi connected but internet test failed");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("No Internet!");
      lcd.setCursor(0, 1);
      lcd.print("Check connection");
      delay(2000);
    }
    
    getNumbers();

    // Use fallback coordinates initially
    latitude = fallback_latitude;
    longitude = fallback_longitude;

    // Get weather data with fallback coordinates
    if (getWeather()) {
      getAISuggestion();
    }

    // Try to get more accurate location from IP
    if (getLocationFromIpInfo()) {
      Serial.println("Successfully obtained location from ipinfo.io");
      if (getWeather()) {
        getAISuggestion();
      }
    } else {
      Serial.println("Using fallback coordinates");
      latitude = fallback_latitude;
      longitude = fallback_longitude;
    }
  } else {
    Serial.println("\nFailed to connect to WiFi");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed");
    lcd.setCursor(0, 1);
    lcd.print("Check settings");
    delay(2000);
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

  // Simplified periodic connectivity check 
  if (currentTime - lastConnectivityCheck >= connectivityCheckInterval) {
    lastConnectivityCheck = currentTime;
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected, attempting to reconnect...");
      
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("WiFi Disconnected");
      
      // Simple reconnect without nesting
      WiFi.disconnect();
      delay(1000);
      WiFi.begin(SSID, PASSWORD);
      
      // Wait up to 5 seconds for connection
      int timeout = 5;
      while (WiFi.status() != WL_CONNECTED && timeout > 0) {
        delay(1000);
        Serial.print(".");
        timeout--;
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi reconnected successfully");
      }
    }
  }

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
  
  // Check and ensure WiFi connection
  if (!ensureWiFiConnection()) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SMS Failed");
    lcd.setCursor(0, 1);
    lcd.print("No WiFi");
    return;
  }
  
  // Update LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sending SMS...");
  lcd.setCursor(0, 1);
  lcd.print("To: " + String(to));
  
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10); // 10 second timeout
  
  Serial.println("🔄 Connecting to SMS API...");
  
  // Connect with timeout control
  bool connected = false;
  unsigned long connectStartTime = millis();
  while (!connected && (millis() - connectStartTime < 5000)) { // 5 second connection timeout
    if (client.connect("api.httpsms.com", 443)) {
      connected = true;
      Serial.println("Connected to HttpSMS API");
    } else {
      delay(500);
    }
  }
  
  if (!connected) {
    Serial.println("Connection to HttpSMS API timed out");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SMS Failed");
    lcd.setCursor(0, 1);
    lcd.print("Connection Error");
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
  
  Serial.println("SMS Request sent, waiting for response...");
  
  // Read response with proper timeout
  String response = "";
  bool responseReceived = false;
  unsigned long responseTimeout = millis() + 5000; // 5 seconds response timeout
  
  while (millis() < responseTimeout) {
    if (client.available()) {
      responseReceived = true;
      char c = client.read();
      response += c;
    } else if (!client.connected() && responseReceived) {
      // Response complete and connection closed
      break;
    }
    delay(10); // Short delay to prevent hogging CPU
  }
  
  client.stop();
  
  if (response.length() > 0) {
    Serial.println("SMS API Response received:");
    Serial.println(response);
    
    // Check if response contains success indicators
    if (response.indexOf("HTTP/1.1 20") > 0) { // HTTP 200 or 201
      Serial.println("✅ SMS sent successfully");
      
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("SMS Sent!");
      lcd.setCursor(0, 1);
      lcd.print("To: " + String(to));
      delay(1000);
    } else {
      Serial.println("❌ SMS sending failed based on response");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("SMS Failed");
      lcd.setCursor(0, 1);
      lcd.print("Server Error");
    }
  } else {
    Serial.println("No response from SMS API or timeout occurred");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SMS Status");
    lcd.setCursor(0, 1);
    lcd.print("Unknown (timeout)");
  }
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
  // Check and ensure WiFi connection
  if (!ensureWiFiConnection()) {
    return false;
  }

  HTTPClient http;
  bool success = false;
  
  // Set timeout but not too long
  http.setTimeout(5000);
  
  // Update LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Getting Location");
  
  // Simple approach - one attempt with proper timeout
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
        
        // Show location on LCD
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Location: " + city);
        lcd.setCursor(0, 1);
        lcd.print(region + ", " + country);
        delay(1000);
      }
    } else {
      Serial.println("Error parsing location data from ipinfo.io");
    }
  } else {
    Serial.print("Failed to get location from IPInfo, HTTP code: ");
    Serial.println(httpCode);
    
    // If API failed due to connectivity, try to reconnect
    if (httpCode < 0) {
      WiFi.disconnect();
      delay(1000);
      WiFi.begin(SSID, PASSWORD);
    }
  }
  
  http.end();
  
  if (!success) {
    Serial.println("Using fallback coordinates");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Location Error");
    lcd.setCursor(0, 1);
    lcd.print("Using defaults");
    delay(1000);
  }

  return success;
}

bool getWeather() {
  // Check and ensure WiFi connection
  if (!ensureWiFiConnection()) {
    return false;
  }

  HTTPClient http;
  bool success = false;
  
  // Set timeout but not too long
  http.setTimeout(5000);
  
  String url = String("http://api.openweathermap.org/data/2.5/weather?q=Caloocan,PH&appid=") + weatherApiKey + "&units=metric&lang=en";

  // Update LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Getting Weather");
  
  Serial.println("Weather API URL: " + url);
  
  // Simple approach - one attempt with proper timeout
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
      
      // Show weather on LCD briefly
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(cityName + ": " + String(temperature, 1) + "C");
      lcd.setCursor(0, 1);
      lcd.print(weatherDescription);
      delay(1000);
    } else {
      Serial.println("Error parsing weather data");
    }
  } else {
    Serial.println("Failed to connect to OpenWeather API, HTTP code: " + String(httpCode));
    
    // If API failed due to connectivity, try to reconnect
    if (httpCode < 0) {
      WiFi.disconnect();
      delay(1000);
      WiFi.begin(SSID, PASSWORD);
    }
  }
  
  http.end();
  
  // If we failed, use fallback data
  if (!success) {
    Serial.println("Using fallback weather data");
    
    // Provide fallback data
    location = "Caloocan";
    weatherDescription = "unknown weather";
    temperature = 30.0;  // Reasonable default for Philippines
    feelsLike = 32.0;
    humidity = 70.0;
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Weather Error");
    lcd.setCursor(0, 1);
    lcd.print("Using defaults");
    delay(1000);
    
    // We return true even with fallback data to allow the program to continue
    return true;
  }

  return success;
}

void getAISuggestion() {
  // Check and ensure WiFi connection
  if (!ensureWiFiConnection()) {
    // Use fallback message if we can't connect
    AISuggestion = "PRAF Technology Weather Update: Walang internet connection. Mag-ingat pa rin sa posibleng pagbaha sa panahon ng ulan.";
    return;
  }

  // Update LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Getting AI Data");
  lcd.setCursor(0, 1);
  lcd.print("Please wait...");

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

  // Try up to 3 times with full error handling
  bool success = false;
  int maxAttempts = 3;
  
  for (int attempt = 0; attempt < maxAttempts && !success; attempt++) {
    if (attempt > 0) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("AI Retry #" + String(attempt));
      delay(1000);
    }
    
    Serial.print("Attempt ");
    Serial.print(attempt + 1);
    Serial.println(" connecting to Gemini API...");
    
    // Create a secure client for HTTPS
    WiFiClientSecure secureClient;
    secureClient.setInsecure(); // Skip certificate validation
    
    HTTPClient http;
    http.setTimeout(15000); // Set longer timeout for Gemini API
    
    // Use WiFiClientSecure with begin
    http.begin(secureClient, geminiUrl);
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
        
        // Show success on LCD
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("AI Data Received");
        delay(1000);
        
        success = true;
      } else {
        Serial.println("Error parsing Gemini API response");
      }
    } else {
      Serial.print("Failed to connect to Gemini API, HTTP code: ");
      Serial.println(httpCode);
      
      // For error code -1 (connection refused), try to troubleshoot
      if (httpCode == -1) {
        troubleshootHTTPConnection(httpCode, "generativelanguage.googleapis.com");
        
        // Wait a bit longer between retries after troubleshooting
        delay(3000);
      } else {
        // For other errors, just wait a bit before retrying
        delay(1000);
      }
    }
    
    http.end();
  }
  
  // If all attempts failed, use fallback
  if (!success) {
    prepareFallbackAISuggestion();
  }
}

// Helper function to prepare a fallback AI suggestion when the API call fails
void prepareFallbackAISuggestion() {
  // Prepare a simple fallback message based on current weather
  if (weatherDescription.indexOf("rain") >= 0) {
    AISuggestion = "PRAF Technology Weather Update: Umuulan sa " + location + 
                   ", maaring magkaroon ng pagbaha sa mababang lugar. Mag-ingat at maghanda sa posibleng pagbaha.";
  } else {
    AISuggestion = "PRAF Technology Weather Update: Ang panahon sa " + location + 
                   " ay " + weatherDescription + " na may temperatura ng " + String(temperature, 1) + 
                   "°C. Mag-ingat at uminom ng maraming tubig.";
  }
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Using Backup Data");
  delay(1000);
}

void getNumbers() {
  // Check and ensure WiFi connection
  if (!ensureWiFiConnection()) {
    return;
  }

  // Clear the existing phone numbers array
  registeredPhoneNumbers.clear();

  // Update LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Getting Numbers");
  
  // Try up to 3 times with error handling
  bool success = false;
  int maxAttempts = 3;
  
  for (int attempt = 0; attempt < maxAttempts && !success; attempt++) {
    if (attempt > 0) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Numbers Retry #" + String(attempt));
      delay(1000);
    }
    
    Serial.print("Attempt ");
    Serial.print(attempt + 1);
    Serial.println(" fetching numbers from Supabase...");
    
    // Create a secure client for HTTPS
    WiFiClientSecure secureClient;
    secureClient.setInsecure(); // Skip certificate validation
    
    HTTPClient http;
    http.setTimeout(10000); // Increase timeout
    
    String endpoint = String(supabaseUrl) + "/rest/v1/" + tableName;
    
    // Use WiFiClientSecure with begin
    http.begin(secureClient, endpoint);
    http.addHeader("apikey", supabaseKey);
    http.addHeader("Authorization", "Bearer " + String(supabaseKey));
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Prefer", "return=representation");
    
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
        
        // Show success on LCD
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Numbers Loaded");
        lcd.setCursor(0, 1);
        lcd.print("Total: " + String(registeredPhoneNumbers.size()));
        delay(1000);
        
        success = true;
      }
    } else {
      Serial.print("Error getting entries. HTTP Response code: ");
      Serial.println(httpResponseCode);
      
      // For error code -1 (connection refused), try to troubleshoot
      if (httpResponseCode == -1) {
        troubleshootHTTPConnection(httpResponseCode, "supabase.co");
        
        // Wait a bit longer between retries after troubleshooting
        delay(3000);
      } else {
        // For other errors, just wait a bit before retrying
        delay(1000);
      }
    }
    
    http.end();
  }
  
  Serial.println("Registered phone numbers:");
  for (const String& number : registeredPhoneNumbers) {
    Serial.println(number);
  }
}

// CHECKPOINT #2

bool checkInternetConnection() {
  HTTPClient http;
  http.setTimeout(5000); // 5 second timeout
  
  // Use a lightweight endpoint
  const char* testUrl = "http://www.google.com";
  
  Serial.println("Testing internet connectivity...");
  http.begin(testUrl);
  int httpCode = http.GET();
  http.end();
  
  if (httpCode == HTTP_CODE_OK) {
    Serial.println("✅ Internet connection available");
    return true;
  } else {
    Serial.print("❌ Internet connection test failed, HTTP code: ");
    Serial.println(httpCode);
    return false;
  }
}

void reconnectWiFi() {
  // Don't try to reconnect if we're already trying to connect
  static bool reconnecting = false;
  if (reconnecting) return;
  
  reconnecting = true;
  
  // Simple approach - disconnect and reconnect
  Serial.println("Reconnecting to WiFi...");
  
  // Update LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Reconnect");
  
  // Disconnect first
  WiFi.disconnect();
  delay(1000);
  
  // Try to connect
  WiFi.begin(SSID, PASSWORD);
  
  // Wait up to 10 seconds
  int timeout = 10;
  while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(1000);
    Serial.print(".");
    lcd.setCursor(10-timeout, 1);
    lcd.print(".");
    timeout--;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi reconnected successfully");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    delay(1000);
  } else {
    Serial.println("\nWiFi reconnection failed");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed");
    lcd.setCursor(0, 1);
    lcd.print("Check settings");
  }
  
  reconnecting = false;
}

// Simple function to ensure WiFi is connected before API calls
bool ensureWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    reconnectWiFi();
    return (WiFi.status() == WL_CONNECTED);
  }
  return true;
}
