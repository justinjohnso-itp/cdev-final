#include <Arduino.h>
#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include "secrets.h" // Include the file with WiFi credentials

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

// HTTP Client Objects
WiFiSSLClient wifiClient; // Use SSL for HTTPS
HttpClient httpClient = HttpClient(wifiClient, serverAddress, serverPort);

// --- Function Declarations ---
void connectToWiFi();
void fetchSpotifyData();

// --- Setup Function ---
void setup() {
  Serial.begin(9600);
  Serial.println("Spotify Data Fetcher Starting...");
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
  Serial.println("\\nFetching Spotify data...");

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
      return;
    }
    bool isPlaying = doc["isPlaying"].as<bool>();
    if (isPlaying) {
      Serial.println("Music is playing.");
    } else {
      Serial.println("Music is not playing.");
    }
  } else if (httpStatusCode == 401) {
    Serial.println("Server returned 401 Unauthorized. Check server logs. Is user logged in?");
  } else {
    Serial.print("HTTP request failed with code: ");
    Serial.println(httpStatusCode);
    Serial.println("Response: ");
    Serial.println(responseBody);
  }
}