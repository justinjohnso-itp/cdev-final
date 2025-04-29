#include <Adafruit_NeoMatrix.h> // For 2D matrix text and pixel control
#include <Arduino.h>
#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h> // For text scrolling (requires Adafruit_GFX library)
#include "secrets.h" // Include the file with WiFi credentials

// --- Configuration ---
// NeoPixel Config
#define LED_PIN   2   // Digital pin connected to the NeoPixels
#define LED_COUNT 256 // Number of NeoPixels

// WiFi Credentials (from secrets.h)
const char* ssid = WIFI_SSID;
const char* pass = WIFI_PASS;

// Server Config (Update with your server's IP/hostname if needed)
// For Netlify, use the domain only (no protocol, no trailing slash)
const char* serverAddress = "cdev-spotify-visualizer.netlify.app";
const int serverPort = 443; // HTTPS port

// Polling Interval (how often to check Spotify)
const unsigned long pollingInterval = 5000; // 5 seconds
unsigned long lastPollTime = 0;

// --- Global Variables ---
int status = WL_IDLE_STATUS; // WiFi status

// Replace Adafruit_NeoPixel matrix with Adafruit_NeoMatrix for 8x32 display
// Adafruit_NeoPixel matrix(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(32, 8, LED_PIN,
  NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG,
  NEO_GRB + NEO_KHZ800);

// HTTP Client Objects
WiFiSSLClient wifiClient; // Use SSL for HTTPS
HttpClient httpClient = HttpClient(wifiClient, serverAddress, serverPort);

// --- Function Declarations ---
void connectToWiFi();
void fetchSpotifyData();
void updateLEDs(const JsonDocument& features);
void clearLEDs();
void colorWipe(uint32_t color, int wait); // Added for startup animation
void runMatrixOrientationTest();

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

  // --- Run orientation and text test before WiFi/Serial ---
  runMatrixOrientationTest();

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
  Serial.print("Requesting URL: https://");
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

// Helper: Convert [r,g,b] to uint32_t color (for ArduinoJson 6+)
uint32_t rgbToColor(const JsonArrayConst& arr) {
  if (arr.size() != 3) return matrix.Color(255, 255, 255);
  return matrix.Color(arr[0], arr[1], arr[2]);
}

// Helper: Get color from palette (wraps if idx >= size)
uint32_t paletteColor(const JsonArrayConst& palette, int idx) {
  if (palette.size() == 0) return matrix.Color(255, 255, 255);
  int safeIdx = idx % palette.size();
  return rgbToColor(palette[safeIdx].as<JsonArrayConst>());
}

// Custom mapping for 8x32 snaked, chunked matrix (8x8 panels)
int mapXY(int x, int y) {
  // Each chunk is 8 columns wide
  int chunk = x / 8;
  int chunk_x = x % 8;
  int base = chunk * 64; // 8 cols * 8 rows = 64 LEDs per chunk

  // Within each chunk, columns snake: even col goes down, odd goes up
  int col = chunk_x;
  int row = y;
  int idx;
  if (col % 2 == 0) {
    // Even column: top to bottom
    idx = base + col * 8 + row;
  } else {
    // Odd column: bottom to top
    idx = base + col * 8 + (7 - row);
  }
  return idx;
}

// Helper: Set a pixel using the custom mapping
void setMatrixPixel(int x, int y, uint32_t color) {
  matrix.setPixelColor(mapXY(x, y), color);
}

// --- Update LEDs with palette and scrolling text ---
void updateLEDs(const JsonDocument& doc) {
  bool isPlaying = doc["isPlaying"];
  long progress_ms = doc["progress_ms"] | 0;
  long duration_ms = doc["duration_ms"] | 1;
  float progress = (float)progress_ms / (float)duration_ms;
  const char* trackName = doc["track"]["name"] | "";
  const char* artistName = doc["track"]["artists"][0] | "";
  
  // Parse palette and dominantColor as JsonArrayConst
  JsonArrayConst palette = doc["track"]["palette"].as<JsonArrayConst>();
  JsonArrayConst dominant = doc["track"]["dominantColor"].as<JsonArrayConst>();

  // --- 1. Draw scrolling text on row 0 of the main matrix ---
  static int16_t scrollX = 32;
  static unsigned long lastScroll = 0;
  String scrollText = String(trackName) + " - " + String(artistName) + "   ";

  // Clear the matrix before drawing
  matrix.clear();

  // Draw progress bar and color waves on rows 1-7 using custom mapping
  int barRows = 7; // Use rows 1-7 for visuals
  int barCols = 32;
  int numLit = (int)(progress * barCols);
  for (int y = 1; y <= barRows; y++) {
    for (int x = 0; x < barCols; x++) {
      if (x < numLit) {
        // Use palette color for each row
        uint32_t color = paletteColor(palette, y-1);
        setMatrixPixel(x, y, color);
      } else {
        setMatrixPixel(x, y, 0);
      }
    }
  }

  // --- Serial Output for Debugging ---
  Serial.print("Track: "); Serial.println(trackName);
  Serial.print("Artist: "); Serial.println(artistName);
  Serial.print("Progress: "); Serial.print(progress * 100, 1); Serial.println("%");

  // Show the result
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

void runMatrixOrientationTest() {
  // Fill the matrix pixel by pixel, starting at (0,0) and progressing in logical order
  matrix.clear();
  matrix.setBrightness(100);
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 32; x++) {
      setMatrixPixel(x, y, matrix.Color(255, 0, 0));
      matrix.show();
      delay(20); // Short delay to see the fill order
    }
  }
  delay(1000);
  matrix.clear();
  matrix.show();
  // Draw green at (0,0), blue at (31,7) for reference
  setMatrixPixel(0, 0, matrix.Color(0,255,0));
  setMatrixPixel(31, 7, matrix.Color(0,0,255));
  matrix.show();
  delay(2000);
  matrix.clear();
  matrix.show();
}