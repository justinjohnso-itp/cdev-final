# Building the Spotify LED Visualizer: Getting the Arduino Talking

*April 21, 2025*

So, the idea for this project is a physical LED strip visualizer that reacts to whatever music is playing on Spotify. The twist is that I want *anyone* to be able to walk up, scan a QR code, log into *their* Spotify account via a web page, and have the lights sync to *their* music.

## Project Structure

I decided to break this down into three main parts:

1.  **Arduino (Nano 33 IoT):** This is the physical device. It connects to WiFi, controls the WS2812B LED strip (using the Adafruit NeoPixel library), and periodically asks the server for data about the currently playing song.
2.  **Server (Node.js/Express):** This acts as the middleman. It handles the Spotify OAuth 2.0 authentication flow, securely stores user tokens (using a simple file-based JSON DB for now), talks to the Spotify API to get playback state and audio features, and provides API endpoints for both the web client and the Arduino.
3.  **Client (Astro):** A simple web frontend. Its main job is to display a QR code (linking to the server's login route) and maybe show some status information. When a user scans the QR code, they get redirected to Spotify to authorize the app, and then back to the server's callback endpoint.

The core interaction flow looks like this:
Arduino -> Server -> Spotify API
User -> Client (QR) -> Server -> Spotify Auth -> Server (Callback) -> Client (Redirect)

## Setting Up the Pieces

### Server-Side Shenanigans: Authentication is Tricky

I started with the server, setting up a basic Express app. The key part was integrating with the Spotify API. I used the `spotify-web-api-node` library, which simplifies things a lot.

Setting up the OAuth flow (`/auth/login` redirecting to Spotify, `/auth/callback` handling the response) was relatively straightforward following the [Spotify Authorization Code Flow guide](https://developer.spotify.com/documentation/web-api/tutorials/code-flow).

```javascript
// web/server/src/controllers/authController.js (Initial callback logic)
exports.callback = async (req, res) => {
  // ... handle state verification, errors ...
  const data = await spotifyApi.authorizationCodeGrant(code);
  const accessToken = data.body['access_token'];
  const refreshToken = data.body['refresh_token'];
  const expiresAt = Date.now() + data.body['expires_in'] * 1000;

  // Get user ID
  const userData = await spotifyApi.getMe();
  const userId = userData.body.id;

  // Save tokens to DB (using my db.js utility)
  await db.saveTokens(userId, accessToken, refreshToken, expiresAt);

  // Store user ID in session
  req.session.userId = userId;

  // Redirect back to the client app
  res.redirect(process.env.CLIENT_URL || 'http://localhost:4321');
};
```

I added session management (`express-session`) so the server remembers who's logged in. Then I created API endpoints like `/api/currently-playing` and `/api/audio-features`. To protect these, I wrote an authentication middleware (`requireAuth`).

```javascript
// web/server/src/middleware/authMiddleware.js (Initial version)
const requireAuth = async (req, res, next) => {
  const userId = req.session.userId;
  if (!userId) {
    return res.status(401).json({ error: "Authentication required." });
  }
  // ... fetch tokens from DB for userId ...
  // ... check if token expired, refresh if needed ...
  // ... attach configured spotifyApi client to req ...
  next();
};

// web/server/src/routes/api.js (Initial version)
router.get("/audio-features", requireAuth, async (req, res) => {
  // Use req.spotifyApi attached by middleware
  // ... fetch features ...
});
```

### Arduino: Hitting a Wall

Next, I worked on the Arduino code. Using the `WiFiNINA` library, I got it connected to my network. I used `ArduinoHttpClient` to make requests to the server.

```cpp
// arduino/src/main.cpp (Initial attempt)
#include <ArduinoHttpClient.h>
#include <WiFiNINA.h>
// ... other includes ...

const char* serverAddress = "YOUR_SERVER_IP"; // Needs local IP
const int serverPort = 3000;
WiFiClient wifiClient;
HttpClient httpClient = HttpClient(wifiClient, serverAddress, serverPort);

void fetchSpotifyData() {
  if (WiFi.status() != WL_CONNECTED) return;

  String apiPath = "/api/audio-features"; // <-- The protected endpoint
  httpClient.beginRequest();
  int err = httpClient.get(apiPath); // Attempt to GET data
  httpClient.endRequest();

  if (err == 0) {
    int httpStatusCode = httpClient.responseStatusCode();
    if (httpStatusCode == 200) {
      String responseBody = httpClient.responseBody();
      // ... parse JSON with ArduinoJson ...
      // ... update LEDs ...
    } else {
      Serial.print("HTTP Error: ");
      Serial.println(httpStatusCode); // <-- Likely getting 401 Unauthorized here
    }
  } else {
    Serial.print("HTTP Request failed, error: ");
    Serial.println(err);
  }
}

void loop() {
  // ... check WiFi ...
  // Poll server every 5 seconds
  if (millis() - lastPollTime >= 5000) {
    lastPollTime = millis();
    fetchSpotifyData();
  }
}
```

This is where the core problem surfaced. The Arduino code tries to hit `/api/audio-features`, but the server's `requireAuth` middleware kicks in. Since the Arduino isn't sending any session cookies, `req.session.userId` is undefined, and the server correctly responds with a 401 Unauthorized. The Arduino can't get the data it needs.

### Rethinking Server Auth for the Device

I needed a way for the Arduino to get data *without* needing its own session cookie, but the data still needed to be for the *currently active user* (whoever last logged in via the web client).

My solution was to introduce a simple global variable on the server (`index.js`) to track the ID of the last successfully authenticated user.

```javascript
// web/server/index.js
// ... imports ...
let activeSpotifyUserId = null; // Stores the Spotify ID of the last authenticated user
const globalState = { activeSpotifyUserId };
module.exports.globalState = globalState; // Export for controllers/routes
// ... rest of server setup ...
```

I modified the `/auth/callback` controller to update this global variable after a successful login and token save.

```javascript
// web/server/src/controllers/authController.js
const { globalState } = require("../../index"); // Import global state

exports.callback = async (req, res) => {
  // ... existing code ...
  try {
    // ... save tokens, set session ...

    // *** Update the active user ID in global state ***
    globalState.activeSpotifyUserId = userId;
    console.log(`Set activeSpotifyUserId to: ${globalState.activeSpotifyUserId}`);

    // ... redirect ...
  } catch (err) { /* ... */ }
};
```

Then, I created a *new*, unauthenticated endpoint specifically for the Arduino: `GET /api/device/data`. This endpoint reads the `activeSpotifyUserId` from the global state.

To avoid duplicating the token fetching/refreshing logic already in my `authMiddleware`, I extracted that logic into a reusable helper function `getAuthenticatedSpotifyApi(userId)` in `src/utils/tokenHelper.js`. This function takes a user ID, gets their tokens from the DB, refreshes if necessary (and saves the new ones), and returns a ready-to-use `SpotifyWebApi` client instance.

```javascript
// web/server/src/utils/tokenHelper.js (Simplified structure)
async function getAuthenticatedSpotifyApi(userId) {
  // ... handle missing userId ...
  // ... get tokens from db.getTokens(userId) ...
  // ... check expiration ...
  // ... if expired, refresh using db tokens and save new ones via db.saveTokens(...) ...
  // ... return new SpotifyWebApi({ accessToken, refreshToken }) ...
}
module.exports = { getAuthenticatedSpotifyApi };
```

I refactored `authMiddleware.js` to use this helper, making it much cleaner.

Finally, I implemented the `/api/device/data` route:

```javascript
// web/server/src/routes/api.js
const { globalState } = require("../../index");
const { getAuthenticatedSpotifyApi } = require("../utils/tokenHelper");

router.get("/api/device/data", async (req, res) => {
  const activeUserId = globalState.activeSpotifyUserId;

  if (!activeUserId) {
    // No user has logged in via the web yet
    return res.json({ isPlaying: false, features: null });
  }

  try {
    // Get a client configured for the active user
    const spotifyApi = await getAuthenticatedSpotifyApi(activeUserId);
    if (!spotifyApi) {
      // Token fetch/refresh failed
      return res.json({ isPlaying: false, features: null });
    }

    // Use the client to get playback state and features
    const playbackState = await spotifyApi.getMyCurrentPlaybackState();
    // ... check if playing ...

    const trackId = playbackState.body.item.id;
    const featuresData = await spotifyApi.getAudioFeaturesForTrack(trackId);
    // ... check for features data ...

    // Construct and return the response
    const features = { /* ... */ };
    res.json({ isPlaying: true, features: features });

  } catch (err) {
    console.error(`Device Endpoint Error: ${err}`);
    res.json({ isPlaying: false, features: null }); // Simplify error for device
  }
});
```

### Updating the Arduino (Again)

The last step was simple: update the Arduino's `fetchSpotifyData` function to call the new `/api/device/data` endpoint instead of the old protected one.

```cpp
// arduino/src/main.cpp
void fetchSpotifyData() {
  // ... connect WiFi ...

  // *** Use the new unauthenticated endpoint ***
  String apiPath = "/api/device/data";
  httpClient.beginRequest();
  int err = httpClient.get(apiPath);
  httpClient.endRequest();

  // ... handle response (same JSON parsing and LED logic) ...
}
```

Now, the Arduino polls the open `/api/device/data` endpoint. The server looks up the `activeSpotifyUserId`, handles getting valid tokens for that user, fetches the data from Spotify, and sends it back to the Arduino. When a new user logs in via the web client, the server updates `activeSpotifyUserId`, and the Arduino automatically starts getting data for the new user on its next poll. Much better.

### Client-Side (Briefly)

The client is an Astro site. For now, it's super basic. It mainly needs to:
1.  Generate a QR code pointing to the server's `/auth/login` route. I used a library like `qrcode` on the server to generate this and passed it to the Astro page, or potentially generate it client-side with JavaScript.
2.  Maybe display some status info later (like who is currently logged in).

The core logic is really on the server and Arduino for this project.

This setup feels much more robust and actually meets the goal of letting anyone control the visualizer by logging into their own Spotify account.
