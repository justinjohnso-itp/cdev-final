#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include "secrets.h" // Include the file with WiFi credentials

// --- Configuration ---
// NeoPixel/NeoMatrix Config
#define LED_PIN    2 // Pin connected to the DIN of the matrix
#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 8
#define LED_COUNT (MATRIX_WIDTH * MATRIX_HEIGHT) // 256 LEDs total
#define MAX_BRIGHTNESS 8 // Brightness limit (0-255) - Increased by 20% from 5

// Parameter 1 = width of NeoPixel matrix
// Parameter 2 = height of matrix
// Parameter 3 = pin number (most are valid)
// Parameter 4 = matrix layout flags, add together as needed:
//   NEO_MATRIX_TOP, NEO_MATRIX_BOTTOM, NEO_MATRIX_LEFT, NEO_MATRIX_RIGHT:
//     Position of the FIRST LED in the matrix; start corner.
//   NEO_MATRIX_ROWS, NEO_MATRIX_COLUMNS: LEDs are arranged in horizontal
//     rows or in vertical columns, respectively; major axis.
//   NEO_MATRIX_PROGRESSIVE, NEO_MATRIX_ZIGZAG: all rows/columns proceed
//     in the same order, or alternate lines reverse direction; minor axis.
//   See example below for these values in action.
// Parameter 5 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(MATRIX_WIDTH, MATRIX_HEIGHT, LED_PIN,
  NEO_MATRIX_TOP     + NEO_MATRIX_LEFT +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB            + NEO_KHZ800);

// WiFi Credentials (from secrets.h)
const char* ssid = WIFI_SSID;
const char* pass = WIFI_PASS;

// Server Config (Update with your server's IP/hostname if needed)
// For Netlify, use the domain only (no protocol, no trailing slash)
const char* serverAddress = "cdev-spotify-visualizer.netlify.app";
const int serverPort = 443; // HTTPS port

// Polling Interval (how often to check Spotify)
const unsigned long pollingInterval = 30000; // 3 seconds (Changed from 5000)
unsigned long lastPollTime = 0;

// --- Global Variables ---
int status = WL_IDLE_STATUS; // WiFi status

String currentTrackName = "";
int scrollOffset = MATRIX_WIDTH; // Start text off-screen to the right
int textWidthPixels = 0; // To store calculated width of the track name

// Variables for smooth playback tracking
bool isPlayingLocally = false; // Our local state of playback
unsigned long currentTrackDurationMs = 0;
unsigned long currentTrackProgressMs = 0;
unsigned long lastSyncTimeMs = 0; // millis() time when progress was last synced

// HTTP Client Objects
WiFiSSLClient wifiClient; // Use SSL for HTTPS
HttpClient httpClient = HttpClient(wifiClient, serverAddress, serverPort);

// --- Function Declarations ---
void connectToWiFi();
void fetchSpotifyData();
void updateLEDs(const JsonDocument& doc);
void clearMatrix();

// --- Setup Function ---
void setup() {
  Serial.begin(9600);
  Serial.println("Spotify Data Fetcher Starting...");

  matrix.begin();
  matrix.setTextWrap(false); // Important for scrolling text
  matrix.setBrightness(MAX_BRIGHTNESS);
  matrix.setTextColor(matrix.Color(255, 255, 255)); // Default text color (white)
  matrix.setTextSize(1); // Use default 5x7 font
  matrix.fillScreen(0); // Start with LEDs off
  matrix.show();

  connectToWiFi();
}

// --- Main Loop ---
void loop() {
  unsigned long currentTime = millis();

  // Check WiFi connection and reconnect if necessary
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Attempting to reconnect...");
    connectToWiFi();
    // Reset local playback state on disconnect
    isPlayingLocally = false;
    currentTrackName = "";
    scrollOffset = MATRIX_WIDTH;
    clearMatrix(); // Clear display immediately
    matrix.show();
    delay(1000); // Add this to avoid hammering WiFi
    lastPollTime = currentTime; // Reset poll timer after reconnect attempt
    return; // Skip the rest of the loop iteration
  }

  // --- Check if song likely ended ---
  bool songLikelyEnded = false;
  if (isPlayingLocally && currentTrackDurationMs > 0) {
    unsigned long estimatedProgress = currentTrackProgressMs + (currentTime - lastSyncTimeMs);
    if (estimatedProgress >= currentTrackDurationMs) {
      songLikelyEnded = true;
      Serial.println("Estimated song end detected.");
    }
  }

  // --- API Polling ---
  // Fetch data immediately if song likely ended, OR if polling interval is up
  if (songLikelyEnded || (currentTime - lastPollTime >= pollingInterval)) {
    if (songLikelyEnded) {
        Serial.println("Triggering immediate fetch due to estimated song end.");
    } else {
        Serial.println("Triggering fetch due to polling interval.");
    }
    lastPollTime = currentTime; // Reset poll timer regardless of trigger reason
    fetchSpotifyData(); // Fetch data
    // After fetching, isPlayingLocally, track name, progress, duration, etc., will be updated.
    // The songLikelyEnded condition will likely become false in the next iteration
    // if a new song started playing and was fetched.
  }

  // --- Scrolling Text Animation (Runs continuously if isPlayingLocally is true) ---
  if (isPlayingLocally && currentTrackName.length() > 0) {
    matrix.fillScreen(0);

    extern JsonArrayConst lastPalette;
    int paletteSize = lastPalette.isNull() ? 0 : lastPalette.size();

    int x = scrollOffset;
    for (int i = 0; i < currentTrackName.length(); i++) {
        matrix.setCursor(x, 1);

        // Calculate color based on gradient logic (simplified for brevity)
        uint16_t color = matrix.Color(255, 255, 255); // Default white
        if (paletteSize > 0) {
             // Simplified: Use first palette color for example
             // Your existing detailed gradient logic goes here
             int totalTextWidth = currentTrackName.length() * 6;
             float pos = float((x + 3) - scrollOffset) / float(totalTextWidth -1); // Approx center of char
             if (pos < 0) pos = 0;
             if (pos > 1) pos = 1;

             uint8_t r, g, b;
             if (paletteSize > 1) {
                float scaled = pos * (paletteSize - 1);
                int idx = int(scaled);
                float frac = scaled - idx;
                uint8_t r1 = lastPalette[idx][0];
                uint8_t g1 = lastPalette[idx][1];
                uint8_t b1 = lastPalette[idx][2];
                uint8_t r2 = lastPalette[min(idx + 1, paletteSize - 1)][0];
                uint8_t g2 = lastPalette[min(idx + 1, paletteSize - 1)][1];
                uint8_t b2 = lastPalette[min(idx + 1, paletteSize - 1)][2];
                r = r1 + (r2 - r1) * frac;
                g = g1 + (g2 - g1) * frac;
                b = b1 + (b2 - b1) * frac;
             } else { // paletteSize == 1
                r = lastPalette[0][0];
                g = lastPalette[0][1];
                b = lastPalette[0][2];
             }
             color = matrix.Color(r, g, b);
        }
        matrix.setTextColor(color);
        matrix.print(currentTrackName[i]);
        x += 6; // Advance cursor for next character (assuming 5x7 font + 1px space)
    }


    scrollOffset--;
    if (scrollOffset < -textWidthPixels) {
      scrollOffset = MATRIX_WIDTH;
    }

    matrix.show();
    delay(80); // Keep animation delay
  } else {
    // If not playing, still delay to prevent busy-waiting
    delay(80);
  }
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
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    WiFi.begin(ssid, pass);
    delay(10000);
  }
  status = WL_CONNECTED; // Set status after successful connection

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
      if (isPlayingLocally) { // Clear only if we thought it was playing
        clearMatrix();
        matrix.show();
      }
      isPlayingLocally = false;
      currentTrackName = "";
      return;
    }

    bool isPlayingApi = doc["isPlaying"].as<bool>();

    if (isPlayingApi) {
      // Music is playing according to API
      if (!isPlayingLocally) {
         Serial.println("Playback started (detected by API).");
         // If playback just started, clear screen before showing text
         clearMatrix();
      }
      isPlayingLocally = true;
      updateLEDs(doc); // Update track info, progress, etc.
    } else {
      // Music is not playing according to API
      if (isPlayingLocally) {
        Serial.println("Playback stopped (detected by API).");
        clearMatrix();
        matrix.show();
        currentTrackName = ""; // Clear track name when stopped
        scrollOffset = MATRIX_WIDTH; // Reset scroll
      }
      isPlayingLocally = false;
    }
  } else {
    // Handle HTTP errors (401, etc.)
    Serial.print("HTTP request failed or unauthorized, code: ");
    Serial.println(httpStatusCode);
    if (isPlayingLocally) { // Clear display if we thought it was playing
        clearMatrix();
        matrix.show();
    }
    isPlayingLocally = false;
    currentTrackName = "";
    scrollOffset = MATRIX_WIDTH;
  }
}

// --- Update LEDs: Handles Track Info and Syncs Progress ---
JsonArrayConst lastPalette; // Add this at the top of your file (global scope)

void updateLEDs(const JsonDocument& doc) {
  // Extract data
  const char* trackNameCStr = doc["track"]["name"] | "Unknown Track";
  String newTrackName = String(trackNameCStr);
  unsigned long newDurationMs = doc["duration_ms"] | 0;
  unsigned long newProgressMs = doc["progress_ms"] | 0;

  // --- Set text color from palette if available ---
  lastPalette = doc["palette"].as<JsonArrayConst>();
  if (!lastPalette.isNull() && lastPalette.size() > 0 && lastPalette[0].size() == 3) {
    uint8_t r = lastPalette[0][0];
    uint8_t g = lastPalette[0][1];
    uint8_t b = lastPalette[0][2];
    matrix.setTextColor(matrix.Color(r, g, b));
  } else {
    matrix.setTextColor(matrix.Color(255, 255, 255)); // Default white
  }

  // Check if track name changed
  if (newTrackName != currentTrackName) {
    Serial.print("New track: ");
    Serial.println(newTrackName);
    currentTrackName = newTrackName;
    scrollOffset = MATRIX_WIDTH; // Reset scroll position for new text

    // Calculate the width of the new text in pixels
    int16_t tempX, tempY;
    uint16_t tempW, tempH;
    matrix.getTextBounds(currentTrackName, 0, 0, &tempX, &tempY, &tempW, &tempH);
    textWidthPixels = tempW; // Store the width

    Serial.print("Text width: ");
    Serial.println(textWidthPixels);
  }

  // Sync playback timing info
  currentTrackDurationMs = newDurationMs;
  currentTrackProgressMs = newProgressMs;
  lastSyncTimeMs = millis(); // Record the time of this sync

  Serial.print("Synced progress: ");
  Serial.print(currentTrackProgressMs);
  Serial.print(" / ");
  Serial.println(currentTrackDurationMs);

  // The main loop handles the drawing based on isPlayingLocally and currentTrackName
}

// Function to turn off all LEDs
void clearMatrix() {
  matrix.fillScreen(0); // Fill with black (off)
}