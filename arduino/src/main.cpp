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
// For Astro API, use the correct port (default 4321) and your computer's IP address
const char* serverAddress = "216.165.95.165"; // <-- Set to your computer's local IP
const int serverPort = 4321; // Astro dev server default port

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
  matrix.setBrightness(10); // Set BRIGHTNESS to 10/255 for startup
  matrix.show();            // Turn OFF all pixels initially

  // Show a simple pattern for 2 seconds (all blue)
  matrix.fill(matrix.Color(0, 0, 255));
  matrix.show();
  delay(2000);
  matrix.clear();
  matrix.show();

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

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Skipping fetch.");
    return;
  }

  // Use Astro API endpoint
  String apiPath = "/api/device/data";
  Serial.print("Requesting URL: http://");
  Serial.print(serverAddress);
  Serial.print(":");
  Serial.print(serverPort);
  Serial.println(apiPath);

  httpClient.beginRequest();
  int err = httpClient.get(apiPath);
  httpClient.endRequest();

  if (err != 0) {
    Serial.print("HTTP GET request failed, error: ");
    Serial.println(err);
    clearLEDs();
    return;
  }

  int httpStatusCode = httpClient.responseStatusCode();
  Serial.print("HTTP Status Code: ");
  Serial.println(httpStatusCode);

  String responseBody = httpClient.responseBody();
  Serial.println("Response Body:");
  Serial.println(responseBody);

  if (httpStatusCode == 200) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, responseBody);
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      clearLEDs();
      return;
    }
    bool isPlaying = doc["isPlaying"].as<bool>();
    if (isPlaying) {
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
    Serial.println(responseBody);
    clearLEDs();
  }
}

// Placeholder for updating LEDs based on audio features - to be implemented later
void updateLEDs(const JsonDocument& doc) {
  // Extract playback info
  bool isPlaying = doc["isPlaying"];
  long progress_ms = doc["progress_ms"] | 0;
  long duration_ms = doc["duration_ms"] | 1; // Avoid division by zero
  const char* trackName = doc["track"]["name"] | "";
  const char* artistName = doc["track"]["artists"][0] | "";
  // Optionally, get albumArt and use a fixed color for now
  // const char* albumArt = doc["track"]["albumArt"] | "";

  // --- Progress Bar Visualization ---
  float progress = (float)progress_ms / (float)duration_ms;
  int numLit = (int)(progress * LED_COUNT);
  uint32_t color = matrix.Color(0, 150, 0); // Green for now, or set based on album art color

  // Light up LEDs for progress
  for (int i = 0; i < LED_COUNT; i++) {
    if (i < numLit) {
      matrix.setPixelColor(i, color);
    } else {
      matrix.setPixelColor(i, 0);
    }
  }
  matrix.show();

  // --- Serial Output for Debugging ---
  Serial.print("Track: "); Serial.println(trackName);
  Serial.print("Artist: "); Serial.println(artistName);
  Serial.print("Progress: "); Serial.print(progress * 100, 1); Serial.println("%");

  // --- (Optional) Display track/artist name on LEDs ---
  // For now, just print to Serial. LED text scrolling would require a font library and more code.
  // You could implement a simple text scroller or use a library like Adafruit_GFX for this.
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