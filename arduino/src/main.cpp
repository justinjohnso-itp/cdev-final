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
#define LED_COUNT (MATRIX_WIDTH * MATRIX_HEIGHT)
#define MAX_BRIGHTNESS 8 // Brightness limit (0-255)
#define SCROLL_SPEED 100 // Default delay (ms) for text scrolling (lower is faster)
#define POT_PIN A0 // Analog pin for potentiometer

// Matrix layout configuration
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
uint16_t idleHue = 0; // Hue for idle animation color cycling (0-65535)
float waveOffset = 0.0; // Offset for scrolling sine wave animation

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
  Serial.print("WiFi SSID: "); Serial.println(ssid);
  // Serial.print("MQTT Broker: "); Serial.print(mqttBroker); Serial.print(":"); Serial.println(mqttPort);

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
  // Serial.println("MQTT message handler configured.");
  // Serial.print("Will subscribe to topic: "); Serial.println(mqttTopic);

  // Set MQTT Broker details
  mqttClient.setId(mqttClientId); // Set Client ID

  // Set username/password ONLY if they are provided in secrets.h
  if (strlen(mqttUser) > 0 && strlen(mqttPassword) > 0) {
      // Serial.println("Setting MQTT username/password.");
      mqttClient.setUsernamePassword(mqttUser, mqttPassword);
  } else {
      // Serial.println("MQTT username/password not set in secrets.h, connecting without authentication.");
  }

  connectToMqtt(); // Initial MQTT connection attempt
  Serial.println("Setup complete: WiFi and MQTT are configured.");
  // Serial.println("MQTT connection and subscription established in setup.");
}

// --- Main Loop ---
void loop() {

  // Serial.println("\n--- Main loop iteration ---");
  // Serial.print("WiFi status: "); Serial.println(WiFi.status());
  // Serial.print("MQTT connected: "); Serial.println(mqttClient.connected());
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
  // Serial.println("--- MQTT status check ---");
  if (!mqttClient.connected()) {
    Serial.println("MQTT disconnected. Attempting to reconnect...");
    connectToMqtt();
  } else {
    // Serial.println("MQTT connected. Polling for messages...");
    mqttClient.poll();
    // Serial.println("Done polling MQTT.");
  }

  // 3. Scrolling Text Animation OR Idle Animation
  if (isPlayingLocally && currentTrackName.length() > 0) {
    // --- Scrolling Text Animation ---
    matrix.fillScreen(0);

    // Read potentiometer for scroll speed
    int potValue = analogRead(POT_PIN);
    // Map the 0-1023 value to a delay range (e.g., 20ms - 200ms)
    // Lower value = faster scroll
    int currentScrollDelay = map(potValue, 0, 1023, 20, 200);

    size_t paletteSize = lastPalette.isNull() ? 0 : lastPalette.size(); // Use size_t
    int x = scrollOffset;
    for (int i = 0; i < currentTrackName.length(); i++) {
        matrix.setCursor(x, 0);

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
    delay(currentScrollDelay); // Use dynamic delay from potentiometer
  } else {
    // --- Idle Animation (Scrolling Sine Wave) ---
    matrix.fillScreen(0); // Clear the screen first

    idleHue += 50; // Slowly cycle hue for the wave color (adjust speed)
    waveOffset += 0.1; // Increment offset to scroll the wave (adjust speed)
    // Reset offset periodically to prevent potential float overflow/precision issues over long time
    if (waveOffset > TWO_PI * 10) { 
        waveOffset -= TWO_PI * 10;
    }

    // Calculate wave parameters
    float amplitude = (MATRIX_HEIGHT / 2.0) - 1.0; // Max amplitude is half height minus 1
    float frequency = TWO_PI / (MATRIX_WIDTH / 1.5); // Adjust denominator for more/fewer waves (e.g., / 1.0 for one wave, / 2.0 for two)
    float verticalCenter = (MATRIX_HEIGHT / 2.0) - 0.5; // Center line for the wave

    // Calculate color for this frame
    uint16_t waveColor = matrix.ColorHSV(idleHue, 255, 255); // Use max saturation/value, rely on global brightness

    // Draw the sine wave
    for (int x = 0; x < MATRIX_WIDTH; x++) {
        // Calculate the vertical position (y) for the wave at this column (x)
        float y_float = amplitude * sin(frequency * x + waveOffset) + verticalCenter;
        int y = round(y_float); // Round to the nearest integer pixel row
        
        // Ensure the calculated y is within the matrix bounds
        y = constrain(y, 0, MATRIX_HEIGHT - 1); 
        
        matrix.drawPixel(x, y, waveColor); // Draw the pixel for the wave
    }

    matrix.show(); // Update the display
    delay(50);     // Delay to control animation speed (adjust as needed, lower is faster)
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
  // Serial.print("Attempting MQTT connection to broker: ");
  // Serial.print(mqttBroker);
  // Serial.print(":");
  // Serial.println(mqttPort);

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
  // Serial.print("mqttClient.connected() = "); Serial.println(mqttClient.connected());

  // Subscribe to the topic
  // Serial.print("Subscribing to topic: ");
  // Serial.println(mqttTopic);
  // subscribe() returns the QoS level granted (0, 1, or 2) on success, 0 on failure (if QoS > 0 requested)
  // For QoS 0, it might return 1 on success? Let's check for non-zero return.
  int subAck = mqttClient.subscribe(mqttTopic);
  // Serial.print("mqttClient.subscribe() returned: "); Serial.println(subAck);
  if (subAck == 0) { // Check if subscription failed (returned 0)
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
  String payload = "";
  payload.reserve(messageSize);
  while (mqttClient.available()) {
    payload += (char)mqttClient.read();
  }

   if (payload.length() == 0) {
    Serial.println("Warning: Received empty payload. Assuming playback stopped.");
    if (isPlayingLocally) { // If it was playing, now it's not.
        currentTrackName = "";
        scrollOffset = MATRIX_WIDTH;
        lastPalette = JsonArrayConst();
    }
    isPlayingLocally = false; // Let main loop handle display based on this state
    return;
  }

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    if (isPlayingLocally) { // If it was playing, now it's not due to error.
        currentTrackName = "";
        scrollOffset = MATRIX_WIDTH;
        lastPalette = JsonArrayConst();
    }
    isPlayingLocally = false; // Let main loop handle display based on this state
    return;
  }

  if (!doc.containsKey("isPlaying")) {
      Serial.println("Error: MQTT message missing 'isPlaying' field. Assuming playback stopped.");
      if (isPlayingLocally) {
        currentTrackName = "";
        scrollOffset = MATRIX_WIDTH;
        lastPalette = JsonArrayConst();
      }
      isPlayingLocally = false; // Let main loop handle display based on this state
      return;
  }

  bool isPlayingApi = doc["isPlaying"].as<bool>();

  if (isPlayingApi) {
    if (!doc.containsKey("track") || !doc["track"].is<JsonObject>() || !doc["track"]["name"].is<const char*>()) {
        Serial.println("Error: MQTT message missing valid track information while isPlaying is true. Assuming playback stopped.");
        if (isPlayingLocally) {
            currentTrackName = "";
            scrollOffset = MATRIX_WIDTH;
            lastPalette = JsonArrayConst();
        }
        isPlayingLocally = false; // Let main loop handle display based on this state
        return;
    }

    // Valid playing message.
    // The main loop's text animation block will handle clearing the screen if it was previously in idle mode.
    // No clearMatrix() here.
    
    // Log if state is actually changing from not playing to playing
    // if (!isPlayingLocally) {
    //    Serial.println("Playback started (detected by MQTT).");
    // }
    isPlayingLocally = true;
    updateLEDs(doc); // This updates currentTrackName, palette, etc.

  } else {
    // Music is not playing according to MQTT message (isPlayingApi is false)
    if (isPlayingLocally) { // If it was playing, now it's stopped.
      Serial.println("Playback stopped (detected by MQTT).");
      currentTrackName = "";
      scrollOffset = MATRIX_WIDTH;
      // Clear other state variables
      currentTrackDurationMs = 0;
      currentTrackProgressMs = 0;
      lastSyncTimeMs = 0;
      lastPalette = JsonArrayConst();
    }
    isPlayingLocally = false;
    // Ensure currentTrackName is clear if it wasn't already (e.g. due to an error path setting isPlayingLocally to false but not clearing name)
    if (currentTrackName != "") { currentTrackName = ""; scrollOffset = MATRIX_WIDTH; }
    // Ensure palette is clear if it wasn't already
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
          // Serial.println("Palette received and seems valid.");
      } else {
          Serial.println("Warning: Received palette is invalid or empty array. Ignoring.");
          // Keep the old palette or clear it? Let's clear it.
          lastPalette = JsonArrayConst();
      }
  } else {
    // No palette field, or it's not a valid array. Clear the existing one.
    if (!lastPalette.isNull()) { // Only print if clearing an existing palette
        // Serial.println("No valid palette in message. Clearing stored palette.");
        lastPalette = JsonArrayConst();
    }
  }

  // Check if track name changed
  if (newTrackName != currentTrackName) {
    // Serial.print("New track: ");
    // Serial.println(newTrackName);
    currentTrackName = newTrackName;
    scrollOffset = MATRIX_WIDTH; // Reset scroll position for new text

    // Calculate the width of the new text in pixels
    int16_t tempX, tempY;
    uint16_t tempW, tempH;
    // Ensure matrix is ready before getting bounds
    matrix.getTextBounds(currentTrackName.c_str(), 0, 0, &tempX, &tempY, &tempW, &tempH);
    textWidthPixels = tempW; // Store the width

    // Serial.print("Text width (using Font5x5_6pt7b): ");
    // Serial.println(textWidthPixels);
  }

  // Sync playback timing info (if provided)
  currentTrackDurationMs = newDurationMs;
  currentTrackProgressMs = newProgressMs;
  lastSyncTimeMs = millis(); // Record the time of this sync

  // Serial.print("Synced progress: ");
  // Serial.print(currentTrackProgressMs);
  // Serial.print(" / ");
  // Serial.println(currentTrackDurationMs);

  // The main loop handles the drawing based on isPlayingLocally and currentTrackName
}

// Function to turn off all LEDs
void clearMatrix() {
  matrix.fillScreen(0); // Fill with black (off)
  // No matrix.show() here, let the caller decide when to update the physical display
  // (e.g., main loop does it during animation, or stop logic does it immediately)
}

// Removed fetchSpotifyData function