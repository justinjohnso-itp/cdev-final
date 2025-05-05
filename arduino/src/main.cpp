#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <WiFiNINA.h>
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include "secrets.h" // Include the file with WiFi and MQTT credentials

// --- Configuration ---
// NeoPixel/NeoMatrix Config
#define LED_PIN    2 // Pin connected to the DIN of the matrix
#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 8
#define LED_COUNT (MATRIX_WIDTH * MATRIX_HEIGHT) // 256 LEDs total
#define MAX_BRIGHTNESS 8 // Brightness limit (0-255)

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

// MQTT Configuration (from secrets.h)
const char* mqttBroker = MQTT_BROKER;
const int mqttPort = MQTT_PORT;
const char* mqttTopic = MQTT_TOPIC;
const char* mqttUser = MQTT_USER; // Optional, depending on broker setup
const char* mqttPassword = MQTT_PASS; // Optional
const char* mqttClientId = "Arduino-SpotifyMatrix"; // Unique client ID

// --- Global Variables ---
int status = WL_IDLE_STATUS; // WiFi status

String currentTrackName = "";
int scrollOffset = MATRIX_WIDTH; // Start text off-screen to the right
int textWidthPixels = 0; // To store calculated width of the track name

// Variables for smooth playback tracking (still relevant if MQTT sends progress)
bool isPlayingLocally = false; // Our local state of playback
unsigned long currentTrackDurationMs = 0;
unsigned long currentTrackProgressMs = 0;
unsigned long lastSyncTimeMs = 0; // millis() time when progress was last synced
JsonArrayConst lastPalette; // Global to store the last received color palette

// WiFi and MQTT Client Objects
WiFiClient wifiClient; // Use plain WiFiClient for non-SSL MQTT
MqttClient mqttClient(wifiClient);

// --- Function Declarations ---
void connectToWiFi();
void connectToMqtt();
void onMqttMessage(int messageSize); // MQTT message handler
void updateLEDs(const JsonDocument& doc);
void clearMatrix();

// --- Setup Function ---
void setup() {
  Serial.begin(9600);
  // while (!Serial); // Optional: Wait for serial connection on native USB ports

  Serial.println("Spotify MQTT Visualizer Starting...");

  matrix.begin();
  matrix.setTextWrap(false); // Important for scrolling text
  matrix.setBrightness(MAX_BRIGHTNESS);
  matrix.setTextColor(matrix.Color(255, 255, 255)); // Default text color (white)
  matrix.setTextSize(1); // Use default 5x7 font
  matrix.fillScreen(0); // Start with LEDs off
  matrix.show();

  connectToWiFi();

  // Set the MQTT message handler *before* connecting
  mqttClient.onMessage(onMqttMessage);

  // Set MQTT Broker details
  mqttClient.setId(mqttClientId); // Set Client ID

  // Set username/password ONLY if they are provided in secrets.h
  if (strlen(mqttUser) > 0 && strlen(mqttPassword) > 0) {
      Serial.println("Setting MQTT username/password.");
      mqttClient.setUsernamePassword(mqttUser, mqttPassword);
  } else {
      Serial.println("MQTT username/password not set in secrets.h, connecting without authentication.");
  }

  connectToMqtt(); // Initial MQTT connection attempt
}

// --- Main Loop ---
void loop() {
  unsigned long currentTime = millis(); // Get current time once per loop

  // 1. Check WiFi Connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Attempting to reconnect...");
    connectToWiFi();
    // Reset local playback state on disconnect
    isPlayingLocally = false;
    currentTrackName = "";
    scrollOffset = MATRIX_WIDTH;
    clearMatrix(); // Clear display immediately
    matrix.show();
    delay(1000); // Avoid hammering WiFi
    return; // Skip the rest of the loop until WiFi is back
  }

  // 2. Check MQTT Connection & Poll
  if (!mqttClient.connected()) {
    // If disconnected, attempt to reconnect MQTT
    Serial.println("MQTT disconnected. Attempting to reconnect...");
    connectToMqtt(); // This function handles delays and retries internally
  } else {
    // If connected, poll for new messages and maintain connection
    mqttClient.poll();
  }

  // 3. Scrolling Text Animation (Runs continuously if isPlayingLocally is true)
  // This logic remains largely the same, driven by isPlayingLocally and currentTrackName
  if (isPlayingLocally && currentTrackName.length() > 0) {
    matrix.fillScreen(0);

    // Use the globally updated lastPalette
    int paletteSize = lastPalette.isNull() ? 0 : lastPalette.size();

    int x = scrollOffset;
    for (int i = 0; i < currentTrackName.length(); i++) {
        matrix.setCursor(x, 1);

        // Calculate color based on gradient logic
        uint16_t color = matrix.Color(255, 255, 255); // Default white
        // Ensure palette is valid and has RGB arrays before using it
        if (paletteSize > 0 && lastPalette[0].is<JsonArrayConst>() && lastPalette[0].size() == 3) {
             int totalTextWidth = currentTrackName.length() * 6; // Approx width
             // Calculate position, ensure denominator is not zero
             float pos = (totalTextWidth > 1) ? float((x + 3) - scrollOffset) / float(totalTextWidth - 1) : 0.0f;
             pos = constrain(pos, 0.0f, 1.0f); // Clamp position between 0 and 1

             uint8_t r, g, b;
             if (paletteSize > 1) {
                float scaled = pos * (paletteSize - 1);
                int idx = int(scaled);
                float frac = scaled - idx;
                // Ensure indices are within bounds
                int idx1 = constrain(idx, 0, paletteSize - 1);
                int idx2 = constrain(idx + 1, 0, paletteSize - 1);

                // Check if palette elements are valid arrays of size 3
                if (lastPalette[idx1].is<JsonArrayConst>() && lastPalette[idx1].size() == 3 &&
                    lastPalette[idx2].is<JsonArrayConst>() && lastPalette[idx2].size() == 3)
                {
                    // Safely access array elements
                    uint8_t r1 = lastPalette[idx1][0].as<uint8_t>();
                    uint8_t g1 = lastPalette[idx1][1].as<uint8_t>();
                    uint8_t b1 = lastPalette[idx1][2].as<uint8_t>();
                    uint8_t r2 = lastPalette[idx2][0].as<uint8_t>();
                    uint8_t g2 = lastPalette[idx2][1].as<uint8_t>();
                    uint8_t b2 = lastPalette[idx2][2].as<uint8_t>();
                    // Interpolate
                    r = r1 + (r2 - r1) * frac;
                    g = g1 + (g2 - g1) * frac;
                    b = b1 + (b2 - b1) * frac;
                } else { // Fallback if palette structure is wrong
                    r = g = b = 255; // White
                }
             } else { // paletteSize == 1 (and we already checked index 0 is valid)
                 r = lastPalette[0][0].as<uint8_t>();
                 g = lastPalette[0][1].as<uint8_t>();
                 b = lastPalette[0][2].as<uint8_t>();
             }
             color = matrix.Color(r, g, b);
        } // else, color remains default white

        matrix.setTextColor(color);
        matrix.print(currentTrackName[i]);
        x += 6; // Advance cursor for next character (assuming 5x7 font + 1px space)
    }

    scrollOffset--;
    // Reset scroll position when text goes fully off screen
    if (scrollOffset < -textWidthPixels) {
      scrollOffset = MATRIX_WIDTH;
    }

    matrix.show();
    delay(80); // Keep animation delay
  } else {
    // If not playing or no track name, ensure LEDs are off
    // (clearMatrix() was called when stopping, but good to be sure)
    // matrix.fillScreen(0); // Optional: ensure screen stays black
    // matrix.show();        // Optional: ensure screen stays black
    delay(50); // Small delay when idle to prevent tight loop
  }
}

// --- Function Definitions ---

void connectToWiFi() {
  // check for the WiFi module
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    while (true); // Halt
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
    // Consider halting or just warning, depending on requirements
  }

  // attempt to connect to WiFi network
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    attempts++;
    Serial.print("Attempting to connect to SSID: ");
    Serial.print(ssid);
    Serial.print(" (Attempt ");
    Serial.print(attempts);
    Serial.println(")");
    WiFi.begin(ssid, pass);
    // Wait 10 seconds for connection:
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
      delay(500);
      Serial.print(".");
    }
    Serial.println(); // Newline after dots or connection message
    if (WiFi.status() != WL_CONNECTED && attempts >= 5) { // Limit retries
        Serial.println("\nFailed to connect to WiFi after multiple attempts. Check credentials/signal.");
        // Optional: Enter a deep sleep mode or blink an error LED
        delay(30000); // Wait before retrying again
        attempts = 0; // Reset attempts after long wait
    }
  }
  status = WL_CONNECTED; // Set status after successful connection

  Serial.println("\nConnected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

// --- Connect to MQTT Broker ---
void connectToMqtt() {
  Serial.print("Attempting MQTT connection to broker: ");
  Serial.print(mqttBroker);
  Serial.print(":");
  Serial.println(mqttPort);

  // Connect to the broker
  // Note: The connect function blocks until connection succeeds or fails
  if (!mqttClient.connect(mqttBroker, mqttPort)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());
    Serial.println("Retrying MQTT connection in 5 seconds...");
    // Wait 5 seconds before retrying
    delay(5000);
    return; // Exit function, will retry in the next loop iteration
  }

  Serial.println("Connected to MQTT Broker!");

  // Subscribe to the topic
  Serial.print("Subscribing to topic: ");
  Serial.println(mqttTopic);
  // subscribe() returns the QoS level granted (0, 1, or 2) on success, 0 on failure (if QoS > 0 requested)
  // For QoS 0, it might return 1 on success? Let's check for non-zero return.
  if (mqttClient.subscribe(mqttTopic) == 0) { // Check if subscription failed (returned 0)
      Serial.print("MQTT subscription failed! Error code = ");
      // Note: ArduinoMqttClient doesn't provide a specific error code for subscribe failure beyond the return value.
      Serial.println(mqttClient.connectError()); // connectError might give related info if connection dropped
      Serial.println("Disconnecting MQTT and retrying connection/subscription...");
      mqttClient.stop(); // Disconnect if subscription fails critically
      delay(5000); // Wait before trying to reconnect everything
  } else {
      Serial.println("Subscription successful!");
  }
}


// --- MQTT Message Handler ---
void onMqttMessage(int messageSize) {
  // We received a message, print the topic and contents
  Serial.println("\nReceived MQTT message!");
  Serial.print("Topic: ");
  Serial.println(mqttClient.messageTopic());

  // Use String to buffer message payload
  String payload = "";
  payload.reserve(messageSize); // Allocate space for efficiency
  while (mqttClient.available()) {
    payload += (char)mqttClient.read();
  }
  Serial.print("Payload (");
  Serial.print(payload.length()); // Use payload.length() instead of messageSize for actual read length
  Serial.print(" bytes): ");
  Serial.println(payload);

  // Check if payload is empty or too small
   if (payload.length() == 0) {
    Serial.println("Warning: Received empty payload.");
    // Decide how to handle empty payload - maybe clear display?
    // clearMatrix();
    // matrix.show();
    // isPlayingLocally = false;
    // currentTrackName = "";
    return; // Exit if no payload
  }

  // Parse the JSON payload
  // Adjust JSON document size as needed based on expected payload size
  // Using dynamic allocation: safer for varying payload sizes but uses heap
  JsonDocument doc;
  // Or use static allocation if max size is known and fits:
  // StaticJsonDocument<1024> doc;

  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    // If parsing fails, maybe the format is wrong or data corrupted
    // Consider clearing the display or logging the raw payload for debugging
    if (isPlayingLocally) { // Clear only if we thought it was playing
      clearMatrix();
      matrix.show();
    }
    isPlayingLocally = false;
    currentTrackName = "";
    lastPalette = JsonArrayConst(); // Clear palette on error
    return; // Exit if JSON is invalid
  }

  // --- Process the received JSON data ---
  // Check for the presence of key fields before accessing them
  if (!doc.containsKey("isPlaying")) {
      Serial.println("Error: MQTT message missing 'isPlaying' field.");
      // Optionally clear display or maintain state
      // clearMatrix(); matrix.show(); isPlayingLocally = false; currentTrackName = "";
      return; // Ignore message if essential data is missing
  }

  bool isPlayingApi = doc["isPlaying"].as<bool>();

  if (isPlayingApi) {
    // Music is playing according to MQTT message
    // Check for essential track info
    if (!doc.containsKey("track") || !doc["track"].is<JsonObject>() || !doc["track"]["name"].is<const char*>()) {
        Serial.println("Error: MQTT message missing valid track information while isPlaying is true.");
        // Optionally clear display or keep previous state, but mark as not playing locally
        if (isPlayingLocally) { clearMatrix(); matrix.show(); }
        isPlayingLocally = false;
        currentTrackName = "";
        lastPalette = JsonArrayConst(); // Clear palette
        return;
    }

    // If playback just started (locally was false, now API is true)
    if (!isPlayingLocally) {
       Serial.println("Playback started (detected by MQTT).");
       clearMatrix(); // Clear screen before showing new text
       // matrix.show() will happen in the main loop's drawing section
    }
    isPlayingLocally = true;
    updateLEDs(doc); // Update track info, progress, palette, etc.

  } else {
    // Music is not playing according to MQTT message
    if (isPlayingLocally) {
      Serial.println("Playback stopped (detected by MQTT).");
      clearMatrix();
      matrix.show(); // Show the cleared screen immediately
      currentTrackName = ""; // Clear track name when stopped
      scrollOffset = MATRIX_WIDTH; // Reset scroll
      // Clear other state variables
      currentTrackDurationMs = 0;
      currentTrackProgressMs = 0;
      lastSyncTimeMs = 0;
      lastPalette = JsonArrayConst(); // Clear the palette
    }
    isPlayingLocally = false;
    // Ensure state is consistent even if it was already false
    if (currentTrackName != "") { currentTrackName = ""; scrollOffset = MATRIX_WIDTH; }
    if (!lastPalette.isNull()) { lastPalette = JsonArrayConst(); }
  }
}


// --- Update LEDs State: Handles Track Info and Syncs Progress (Called from onMqttMessage) ---
// Note: This function updates state variables. The main loop handles drawing.
void updateLEDs(const JsonDocument& doc) {
  // Extract data safely, providing defaults
  const char* trackNameCStr = doc["track"]["name"] | "<No Name>"; // Default if missing
  String newTrackName = String(trackNameCStr);
  // Use .as<T>() which defaults to 0 or false if type mismatch or missing
  unsigned long newDurationMs = doc["duration_ms"].as<unsigned long>();
  unsigned long newProgressMs = doc["progress_ms"].as<unsigned long>();

  // --- Update color palette if available ---
  JsonVariantConst paletteVariant = doc["palette"];
  if (paletteVariant.is<JsonArrayConst>() && !paletteVariant.isNull() && paletteVariant.size() > 0) {
      // Basic validation of the first color entry structure
      if (paletteVariant[0].is<JsonArrayConst>() && paletteVariant[0].size() == 3) {
          lastPalette = paletteVariant.as<JsonArrayConst>();
          Serial.println("Palette received and seems valid.");
      } else {
          Serial.println("Warning: Received palette is invalid or empty array. Ignoring.");
          // Keep the old palette or clear it? Let's clear it.
          lastPalette = JsonArrayConst();
      }
  } else {
    // No palette field, or it's not a valid array. Clear the existing one.
    if (!lastPalette.isNull()) { // Only print if clearing an existing palette
        Serial.println("No valid palette in message. Clearing stored palette.");
        lastPalette = JsonArrayConst();
    }
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
    // Ensure matrix is ready before getting bounds
    matrix.getTextBounds(currentTrackName.c_str(), 0, 0, &tempX, &tempY, &tempW, &tempH);
    textWidthPixels = tempW; // Store the width

    Serial.print("Text width: ");
    Serial.println(textWidthPixels);
  }

  // Sync playback timing info (if provided)
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
  // No matrix.show() here, let the caller decide when to update the physical display
  // (e.g., main loop does it during animation, or stop logic does it immediately)
}

// Removed fetchSpotifyData function