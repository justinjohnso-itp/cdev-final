#include <Arduino.h>
#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include "secrets.h" // Include the file with WiFi credentials

// --- Configuration ---
// NeoPixel Config
#define LED_PIN   2   // Digital pin connected to the NeoPixels
#define LED_COUNT 256 // Number of NeoPixels

// WiFi Credentials (from secrets.h)
const char* ssid = WIFI_SSID;
const char* pass = WIFI_PASS;

// Server Config (Update with your server's IP/hostname if needed)
// If running server locally, use your computer's local IP address
// Find it using `ipconfig` (Windows) or `ifconfig` (macOS/Linux)
const char* serverAddress = "192.168.1.100"; // <-- !!! REPLACE with your server's local IP !!!
const int serverPort = 3000; // Port the Node.js server is running on

// Polling Interval (how often to check Spotify)
const unsigned long pollingInterval = 5000; // 5 seconds
unsigned long lastPollTime = 0;

// --- Global Variables ---
int status = WL_IDLE_STATUS; // WiFi status

// NeoPixel matrix Object
Adafruit_NeoPixel matrix(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// HTTP Client Objects
WiFiClient wifiClient; // TCP client for HTTP
HttpClient httpClient = HttpClient(wifiClient, serverAddress, serverPort);

// --- Function Declarations ---
void connectToWiFi();
void fetchSpotifyData();
void updateLEDs(const JsonDocument& features);
void clearLEDs();
void colorWipe(uint32_t color, int wait); // Added for startup animation

// --- Setup Function ---
void setup() {
  Serial.begin(9600);
  Serial.println("Spotify LED Visualizer Starting...");

  // Initialize NeoPixel matrix
  matrix.begin();           // INITIALIZE NeoPixel matrix object (REQUIRED)
  matrix.setBrightness(10); // Set BRIGHTNESS to 10% (26/255) for startup test
  matrix.show();            // Turn OFF all pixels initially

  // Run startup animation
  Serial.println("Running startup LED animation...");
  colorWipe(matrix.Color(0, 0, 255), 50); // Blue wipe
  colorWipe(matrix.Color(0, 0, 0), 50);   // Clear wipe
  matrix.setBrightness(10); // Reset brightness to default for normal operation (or keep 26 if preferred)
  matrix.show(); // Ensure matrix is off after animation

  // Attempt to connect to WiFi
  connectToWiFi();
}

// --- Main Loop ---
void loop() {
  // Check WiFi connection and reconnect if necessary
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Attempting to reconnect...");
    connectToWiFi();
  }

  // Only fetch data at the specified interval
  unsigned long currentTime = millis();
  if (currentTime - lastPollTime >= pollingInterval) {
    lastPollTime = currentTime;
    fetchSpotifyData();
  }

  // Add any other loop logic here (e.g., handling button presses)
  delay(10); // Small delay to prevent busy-waiting
}

// --- Function Definitions ---

void connectToWiFi() {
  // check for the WiFi module
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to WiFi network
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }

  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

// Function to fetch audio features from the server
void fetchSpotifyData() {
  Serial.println("\nFetching Spotify data...");

  // Make sure we are connected before attempting the request
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Skipping fetch.");
    return;
  }

  // Construct the API path - *** CHANGED TO NEW DEVICE ENDPOINT ***
  String apiPath = "/api/device/data";
  Serial.print("Requesting URL: http://");
  Serial.print(serverAddress);
  Serial.print(":");
  Serial.print(serverPort);
  Serial.println(apiPath);

  // Start the GET request
  httpClient.beginRequest();
  int err = httpClient.get(apiPath);
  httpClient.endRequest();

  if (err != 0) {
    Serial.print("HTTP GET request failed, error: ");
    Serial.println(err);
    clearLEDs(); // Turn off LEDs on error
    return;
  }

  // Get the HTTP status code
  int httpStatusCode = httpClient.responseStatusCode();
  Serial.print("HTTP Status Code: ");
  Serial.println(httpStatusCode);

  // Get the response body
  String responseBody = httpClient.responseBody();
  // Serial.println("Response Body:");
  // Serial.println(responseBody); // Uncomment for debugging, can be large

  if (httpStatusCode == 200) {
    // Allocate JsonDocument. Adjust size as needed based on expected response size.
    // Use https://arduinojson.org/v7/assistant/ to estimate size.
    // Example: {"isPlaying":true,"features":{"id":"...","danceability":0.7,...}}
    // Let's start with 1024, might need adjustment.
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, responseBody);

    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      clearLEDs(); // Turn off LEDs on JSON error
      return;
    }

    // Check if music is playing
    bool isPlaying = doc["isPlaying"].as<bool>();

    if (isPlaying) {
      // Extract the features object
      // Note: We pass the whole doc, updateLEDs will extract features
      Serial.println("Music is playing. Updating LEDs.");
      updateLEDs(doc);
    } else {
      Serial.println("Music is not playing.");
      clearLEDs();
    }
  } else if (httpStatusCode == 401) {
      Serial.println("Server returned 401 Unauthorized. Check server logs. Is user logged in?");
      clearLEDs();
  } else {
    Serial.print("HTTP request failed with code: ");
    Serial.println(httpStatusCode);
    Serial.println("Response: ");
    Serial.println(responseBody); // Print error response body
    clearLEDs(); // Turn off LEDs on non-200 response
  }
}

// Placeholder for updating LEDs based on audio features - to be implemented later
void updateLEDs(const JsonDocument& doc) { // Takes the whole doc now
  // Extract features object
  JsonObjectConst features = doc["features"]; // Corrected: Use JsonObjectConst for read-only access

  if (features.isNull()) {
      Serial.println("Features object is null in JSON response.");
      clearLEDs();
      return;
  }

  Serial.println("Updating LEDs based on features...");

  // Example: Map energy to brightness, danceability to color pattern speed, etc.
  float energy = features["energy"].as<float>();
  float danceability = features["danceability"].as<float>();
  float tempo = features["tempo"].as<float>();
  float valence = features["valence"].as<float>(); // Musical positiveness (0.0-1.0)

  // Simple example: set all LEDs to a color based on valence and brightness based on energy
  int brightness = map(energy * 100, 0, 100, 10, 200); // Map energy (0-1) to brightness (10-200)
  // Map valence (0-1) to hue (e.g., 0=Red, 120=Green, 240=Blue in HSV)
  // Let's map valence to Red (low valence) -> Green (mid) -> Blue (high valence)
  // Hue range is 0-65535 for Adafruit_NeoPixel HSV
  uint16_t hue = map(valence * 100, 0, 100, 0, 65535 / 3 * 2); // Map 0-1 to Red->Green->Blue range

  Serial.print("Energy: "); Serial.print(energy);
  Serial.print(", Valence: "); Serial.print(valence);
  Serial.print(" -> Hue: "); Serial.print(hue);
  Serial.print(", Brightness: "); Serial.println(brightness);

  matrix.fill(matrix.ColorHSV(hue, 255, brightness)); // Full saturation
  matrix.show();
}

// Function to turn off all LEDs
void clearLEDs() {
  Serial.println("Clearing LEDs.");
  matrix.fill(0);
  matrix.show();
}

// Fill matrix pixels one after another with a color.
// Used for startup animation.
void colorWipe(uint32_t color, int wait) {
  for(int i=0; i<matrix.numPixels(); i++) {
    matrix.setPixelColor(i, color);
    matrix.show();
    delay(wait);
  }
}