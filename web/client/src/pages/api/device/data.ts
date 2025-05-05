import ColorThief from 'colorthief';
import fetch from 'node-fetch';
import type { APIRoute } from 'astro';
import { supabase } from '../../../supabase';
import SpotifyWebApi from 'spotify-web-api-node';
import mqtt from 'mqtt'; // Import the MQTT library

// --- Spotify Credentials ---
const CLIENT_ID = import.meta.env.SPOTIFY_CLIENT_ID;
const CLIENT_SECRET = import.meta.env.SPOTIFY_CLIENT_SECRET;
const REDIRECT_URI = import.meta.env.SPOTIFY_REDIRECT_URI;

// --- MQTT Credentials ---
const MQTT_BROKER_URL = import.meta.env.MQTT_BROKER_URL; // e.g., 'mqtts://your_cluster_url.s1.eu.hivemq.cloud'
const MQTT_USERNAME = import.meta.env.MQTT_USERNAME;
const MQTT_PASSWORD = import.meta.env.MQTT_PASSWORD;
const MQTT_TOPIC = import.meta.env.MQTT_TOPIC || 'spotify/visualizer/data'; // Default topic

export const prerender = false;

// --- Helper Functions (getLatestSpotifyTokens, refreshAccessToken - remain the same) ---
async function getLatestSpotifyTokens() {
  const { data, error } = await supabase
    .from('spotify_tokens')
    .select('*')
    .order('last_updated_at', { ascending: false })
    .limit(1)
    .single();
  if (error || !data) return null;
  return data;
}

async function refreshAccessToken(refresh_token: string) {
  const spotifyApi = new SpotifyWebApi({
    clientId: CLIENT_ID,
    clientSecret: CLIENT_SECRET,
    redirectUri: REDIRECT_URI,
    refreshToken: refresh_token,
  });
  const data = await spotifyApi.refreshAccessToken();
  return data.body;
}

// --- Main API Route Handler ---
export const GET: APIRoute = async () => {
  // 1. --- Get Spotify Data (largely the same logic) ---
  const tokens = await getLatestSpotifyTokens();
  if (!tokens) {
    console.error('Error: No Spotify tokens found in Supabase.');
    return new Response(JSON.stringify({ status: 'error', message: 'No Spotify tokens found' }), { status: 404 });
  }
  let { access_token, refresh_token, expires_at, spotify_user_id } = tokens;
  if (new Date(expires_at) < new Date()) {
    console.log('Spotify token expired, refreshing...');
    try {
      const refreshed = await refreshAccessToken(refresh_token);
      access_token = refreshed.access_token;
      expires_at = new Date(Date.now() + refreshed.expires_in * 1000).toISOString();
      await supabase.from('spotify_tokens').update({
        access_token,
        expires_at,
        last_updated_at: new Date().toISOString(),
      }).eq('spotify_user_id', spotify_user_id);
      console.log('Spotify token refreshed successfully.');
    } catch (err: any) {
      console.error('Error refreshing Spotify token:', err.message || err);
      return new Response(JSON.stringify({ status: 'error', message: 'Failed to refresh token' }), { status: 500 });
    }
  }
  const spotifyApi = new SpotifyWebApi({ accessToken: access_token });

  let spotifyDataPayload: Record<string, any>;

  try {
    const playback = await spotifyApi.getMyCurrentPlaybackState();
    if (!playback.body || !playback.body.item) {
      console.log('Spotify not playing or no item found.');
      spotifyDataPayload = { isPlaying: false }; // Send basic 'not playing' state
    } else {
      const track = playback.body.item;
      const albumArt = track.album.images && track.album.images.length > 0 ? track.album.images[0].url : null;

      let dominantColor = null;
      let palette = null;
      if (albumArt) {
        try {
          const response = await fetch(albumArt);
          const arrayBuffer = await response.arrayBuffer();
          const buffer = Buffer.from(arrayBuffer);
          // Note: ColorThief methods might be sync or async depending on version/context
          // Assuming async based on previous usage
          dominantColor = await ColorThief.getColor(buffer);
          palette = await ColorThief.getPalette(buffer, 5); // Get 5 colors
        } catch (e: any) {
          console.warn('Error processing album art:', e.message || e);
          dominantColor = null;
          palette = null;
        }
      }

      spotifyDataPayload = {
        isPlaying: playback.body.is_playing,
        progress_ms: playback.body.progress_ms,
        duration_ms: track.duration_ms,
        timestamp: playback.body.timestamp, // Keep timestamp for potential future use
        track: {
          id: track.id,
          name: track.name,
          artists: track.artists.map((a: any) => a.name),
          album: track.album.name,
          albumArt, // Include album art URL if needed by other clients
        },
        palette: palette, // Send the extracted palette
        // Optional: Include device, shuffle, repeat state if needed by Arduino
        // device: playback.body.device ? { name: playback.body.device.name, type: playback.body.device.type } : null,
        // shuffle_state: playback.body.shuffle_state,
        // repeat_state: playback.body.repeat_state,
      };
      console.log(`Fetched Spotify state: ${spotifyDataPayload.isPlaying ? 'Playing' : 'Paused/Stopped'} - ${spotifyDataPayload.track?.name || 'N/A'}`);
    }
  } catch (err: any) {
    console.error('Error fetching Spotify playback state:', err.message || err);
    // Decide if we should publish an error state or just fail the API call
    // Let's publish a basic 'not playing' state on fetch error
     spotifyDataPayload = { isPlaying: false, error: 'Failed to fetch playback state' };
    // Alternatively, return an error response immediately:
    // return new Response(JSON.stringify({ status: 'error', message: 'Failed to fetch playback info' }), { status: 500 });
  }

  // 2. --- Connect to MQTT and Publish ---
  if (!MQTT_BROKER_URL) {
      console.error('MQTT_BROKER_URL environment variable not set.');
      return new Response(JSON.stringify({ status: 'error', message: 'MQTT broker URL not configured' }), { status: 500 });
  }

  const mqttOptions: mqtt.IClientOptions = {
      clientId: `spotify-api-publisher-${Date.now()}`, // Unique client ID
      username: MQTT_USERNAME,
      password: MQTT_PASSWORD,
      // Add other options like port if not default, rejectUnauthorized, etc.
  };

  console.log(`Connecting to MQTT broker: ${MQTT_BROKER_URL}`);
  const client = mqtt.connect(MQTT_BROKER_URL, mqttOptions);

  // Error storage for MQTT publish process:
  let publishError: any = null;

  // Use a promise to handle async MQTT events within the API route
  await new Promise<void>((resolve, reject) => {
    client.on('connect', () => {
      console.log('MQTT client connected.');
      const message = JSON.stringify(spotifyDataPayload);
      console.log(`Publishing to topic '${MQTT_TOPIC}': ${message.substring(0, 100)}...`); // Log truncated message
      client.publish(MQTT_TOPIC, message, { qos: 0 }, (err) => { // Use QoS 0 for fire-and-forget
        if (err) {
          console.error('MQTT publish error:', err);
          publishError = err;
        } else {
          console.log('MQTT message published successfully.');
        }
        // End the MQTT client connection after publishing
        client.end(true, () => { // Force close after publish attempt
            console.log('MQTT client disconnected.');
            resolve(); // Resolve the promise after disconnecting
        });
      });
    });

    client.on('error', (err) => {
      console.error('MQTT connection error:', err);
      publishError = err; // Store error
      client.end(true, () => { // Attempt to force close on error
          console.log('MQTT client disconnected after error.');
          reject(err); // Reject the promise on connection error
      });
    });

    // Timeout for connection/publish
    setTimeout(() => {
        if (!client.connected && !client.disconnecting) {
            const err = new Error('MQTT connection/publish timeout');
            console.error(err.message);
            publishError = err;
            client.end(true, () => {
                 console.log('MQTT client disconnected after timeout.');
                 reject(err);
            });
        }
    }, 10000); // 10 second timeout
  }).catch(err => {
      // Error already logged, publishError is set
      console.log("Caught MQTT promise rejection.");
  });


  // 3. --- Return API Response ---
  if (publishError) {
    return new Response(JSON.stringify({ status: 'error', message: `Failed to publish to MQTT: ${publishError.message}` }), { status: 500 });
  } else {
    return new Response(JSON.stringify({ status: 'published', data: spotifyDataPayload }), { status: 200 });
  }
};
