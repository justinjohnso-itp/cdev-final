import express from "express";
import dotenv from "dotenv";
import fetch from "node-fetch";
import { createClient } from "@supabase/supabase-js";
import SpotifyWebApi from "spotify-web-api-node";
import mqtt from "mqtt";
import ColorThief from "colorthief";

dotenv.config(); // Load environment variables from .env file

// --- Credentials & Config ---
const {
  PORT,
  SPOTIFY_CLIENT_ID,
  SPOTIFY_CLIENT_SECRET,
  SPOTIFY_REDIRECT_URI,
  SUPABASE_URL,
  SUPABASE_ANON_KEY,
  MQTT_BROKER_URL, // Expecting hostname e.g., "broker.hivemq.com"
  MQTT_USERNAME,
  MQTT_PASSWORD,
  MQTT_TOPIC,
  MQTT_PORT, // Expecting port number e.g., "1883"
} = process.env;

const port = PORT || 3001;
const POLLING_INTERVAL_MS = 2 * 1000; // 5 seconds

// --- Initialize Supabase ---
if (!SUPABASE_URL || !SUPABASE_ANON_KEY) {
  console.error("Supabase URL or Anon Key not configured in .env");
  process.exit(1);
}
const supabase = createClient(SUPABASE_URL, SUPABASE_ANON_KEY);

// --- Initialize Express ---
const app = express();

app.get("/", (req, res) => {
  res.send("Spotify LED Visualizer Server is running!");
});

// --- Initialize MQTT Client (Persistent Connection) ---
let mqttClient = null;

function connectMqtt() {
  if (!MQTT_BROKER_URL || !MQTT_PORT || !MQTT_TOPIC) {
    console.error(
      "MQTT Broker URL, Port, or Topic not configured in .env. Cannot connect."
    );
    return;
  }

  const brokerUrl = `mqtt://${MQTT_BROKER_URL}:${MQTT_PORT}`;
  const mqttClientId = `spotify-server-publisher-${Date.now()}`;
  const options = {
    clientId: mqttClientId,
    username: MQTT_USERNAME,
    password: MQTT_PASSWORD,
    reconnectPeriod: 5000, // Attempt reconnect every 5 seconds if disconnected
    connectTimeout: 10000, // 10 seconds
  };

  console.log(`Attempting to connect to MQTT broker: ${brokerUrl}`);
  mqttClient = mqtt.connect(brokerUrl, options);

  mqttClient.on("connect", () => {
    console.log(`MQTT client connected to ${brokerUrl}`);
    // No need to subscribe here, only publishing
  });

  mqttClient.on("error", (err) => {
    console.error("MQTT Connection Error:", err);
    // The client will attempt to reconnect automatically due to reconnectPeriod
  });

  mqttClient.on("reconnect", () => {
    console.log("MQTT client attempting to reconnect...");
  });

  mqttClient.on("close", () => {
    console.log("MQTT client connection closed. Will attempt to reconnect.");
  });

  mqttClient.on("offline", () => {
    console.log("MQTT client is offline.");
  });

  mqttClient.on("end", () => {
    console.log("MQTT client connection explicitly ended. Reconnecting...");
    // If ended unexpectedly, try to reconnect manually after a delay
    setTimeout(connectMqtt, 5000);
  });
}

// --- Helper Functions (Adapted from Astro API) ---
async function getLatestSpotifyTokens() {
  const { data, error } = await supabase
    .from("spotify_tokens")
    .select("*")
    .order("last_updated_at", { ascending: false })
    .limit(1)
    .single();
  if (error) {
    console.error(
      "Error fetching Spotify tokens from Supabase:",
      error.message
    );
    return null;
  }
  if (!data) {
    console.error("No Spotify tokens found in Supabase.");
    return null;
  }
  return data;
}

async function refreshAccessToken(refresh_token) {
  if (!SPOTIFY_CLIENT_ID || !SPOTIFY_CLIENT_SECRET || !SPOTIFY_REDIRECT_URI) {
    console.error("Spotify API credentials missing in .env for token refresh.");
    throw new Error("Spotify credentials missing");
  }
  const spotifyApi = new SpotifyWebApi({
    clientId: SPOTIFY_CLIENT_ID,
    clientSecret: SPOTIFY_CLIENT_SECRET,
    redirectUri: SPOTIFY_REDIRECT_URI, // May not be strictly needed for refresh, but good practice
    refreshToken: refresh_token,
  });
  try {
    const data = await spotifyApi.refreshAccessToken();
    console.log("Spotify token refreshed successfully.");
    return data.body;
  } catch (err) {
    console.error("Error refreshing Spotify token:", err.message || err);
    throw err; // Re-throw to be caught by the main function
  }
}

// --- Core Logic: Fetch Spotify Data and Publish via MQTT ---
async function fetchAndPublishSpotifyData() {
  console.log(
    `[${new Date().toISOString()}] Starting Spotify data fetch and publish cycle...`
  );

  // 1. Get Spotify Tokens
  const tokens = await getLatestSpotifyTokens();
  if (!tokens) {
    console.error("Halting cycle: Cannot proceed without Spotify tokens.");
    return; // Stop this cycle if tokens aren't available
  }

  let { access_token, refresh_token, expires_at, spotify_user_id } = tokens;

  // 2. Refresh Token if Necessary
  if (new Date(expires_at) < new Date()) {
    console.log("Spotify token expired, attempting refresh...");
    try {
      const refreshed = await refreshAccessToken(refresh_token);
      access_token = refreshed.access_token;
      expires_at = new Date(
        Date.now() + refreshed.expires_in * 1000
      ).toISOString();
      // Update Supabase with the new token and expiry
      const { error: updateError } = await supabase
        .from("spotify_tokens")
        .update({
          access_token,
          expires_at,
          last_updated_at: new Date().toISOString(),
        })
        .eq("spotify_user_id", spotify_user_id);
      if (updateError) {
        console.error(
          "Error updating refreshed token in Supabase:",
          updateError.message
        );
        // Continue with the refreshed token anyway for this cycle, but log the error
      }
    } catch (err) {
      console.error("Halting cycle: Failed to refresh Spotify token.");
      return; // Stop this cycle if refresh fails
    }
  }

  // 3. Fetch Playback State
  const spotifyApi = new SpotifyWebApi({ accessToken: access_token });
  let spotifyDataPayload;

  try {
    const playback = await spotifyApi.getMyCurrentPlaybackState();
    if (!playback.body || !playback.body.item) {
      console.log("Spotify not playing or no item found.");
      spotifyDataPayload = { isPlaying: false };
    } else {
      const track = playback.body.item;
      const albumArt =
        track.album.images && track.album.images.length > 0
          ? track.album.images[0].url
          : null;
      let palette = null;

      if (albumArt) {
        try {
          const response = await fetch(albumArt);
          const arrayBuffer = await response.arrayBuffer();
          const buffer = Buffer.from(arrayBuffer);
          // Using ColorThief synchronously here for simplicity in the server loop
          // If performance becomes an issue, consider worker threads or async versions
          palette = ColorThief.getPalette(buffer, 5); // Get 5 colors
        } catch (e) {
          console.warn("Error processing album art:", e.message || e);
          palette = null;
        }
      }

      spotifyDataPayload = {
        isPlaying: playback.body.is_playing,
        progress_ms: playback.body.progress_ms,
        duration_ms: track.duration_ms,
        timestamp: playback.body.timestamp,
        track: {
          id: track.id,
          name: track.name,
          artists: track.artists.map((a) => a.name),
          album: track.album.name,
        },
        palette: palette,
      };
      console.log(
        `Fetched Spotify state: ${
          spotifyDataPayload.isPlaying ? "Playing" : "Paused/Stopped"
        } - ${spotifyDataPayload.track?.name || "N/A"}`
      );
    }
  } catch (err) {
    console.error("Error fetching Spotify playback state:", err.message || err);
    // Publish a basic 'not playing' state on fetch error to keep the device updated
    spotifyDataPayload = {
      isPlaying: false,
      error: "Failed to fetch playback state",
    };
  }

  // 4. Publish to MQTT (using the persistent client)
  if (!mqttClient || !mqttClient.connected) {
    console.warn(
      "MQTT client not connected. Skipping publish. Will retry connection."
    );
    if (!mqttClient || mqttClient.reconnecting) {
      // If client doesn't exist or is already trying to reconnect, do nothing extra
    } else {
      // Attempt to reconnect if not already doing so
      connectMqtt();
    }
    return; // Skip publishing this cycle
  }

  if (!MQTT_TOPIC) {
    console.error("MQTT_TOPIC not defined in .env. Skipping publish.");
    return;
  }

  console.log(`Publishing to topic: ${MQTT_TOPIC}`);
  mqttClient.publish(
    MQTT_TOPIC,
    JSON.stringify(spotifyDataPayload),
    { qos: 1 }, // Quality of Service 1: At least once delivery
    (err) => {
      if (err) {
        console.error("MQTT Publish Error:", err);
      } else {
        console.log(
          `Successfully published message to ${MQTT_TOPIC}` // Payload: ${JSON.stringify(spotifyDataPayload)}
        );
      }
      // Do NOT end the connection here anymore
      // console.log("MQTT client connection closed after publish.");
    }
  );
}

// --- Start Server and Polling ---
app.listen(port, () => {
  console.log(`Server listening on port ${port}`);
  // Initial MQTT connection
  connectMqtt();

  console.log(
    `Starting Spotify polling every ${POLLING_INTERVAL_MS / 1000} seconds...`
  );
  // Initial fetch
  fetchAndPublishSpotifyData();
  // Set interval for subsequent fetches
  setInterval(fetchAndPublishSpotifyData, POLLING_INTERVAL_MS);
});
