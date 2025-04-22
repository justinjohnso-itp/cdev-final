const express = require("express");
const { requireAuth } = require("../middleware/authMiddleware");

const router = express.Router();

// Example API route protected by authentication middleware
router.get("/currently-playing", requireAuth, async (req, res) => {
  // req.spotifyApi is attached by the requireAuth middleware
  // req.userId is also available
  try {
    const data = await req.spotifyApi.getMyCurrentPlaybackState();

    if (data.body && data.body.is_playing) {
      console.log(
        `User ${req.userId} is currently playing: ${data.body.item.name}`
      );
      // Send relevant track information
      res.json({
        isPlaying: true,
        item: {
          name: data.body.item.name,
          artists: data.body.item.artists.map((artist) => artist.name),
          album: data.body.item.album.name,
          albumArtUrl: data.body.item.album.images[0]?.url, // Get largest image URL
          id: data.body.item.id,
          uri: data.body.item.uri,
        },
        progress_ms: data.body.progress_ms,
        duration_ms: data.body.item.duration_ms,
      });
    } else {
      console.log(`User ${req.userId} is not currently playing anything.`);
      res.json({ isPlaying: false });
    }
  } catch (err) {
    console.error(
      `Error fetching currently playing track for ${req.userId}:`,
      err
    );
    // Handle potential rate limiting or other API errors
    if (err.statusCode === 429) {
      return res
        .status(429)
        .json({ error: "Spotify API rate limit exceeded." });
    }
    res.status(500).json({ error: "Failed to fetch currently playing track." });
  }
});

// Route to get audio features for the currently playing track
router.get("/audio-features", requireAuth, async (req, res) => {
  try {
    // First, get the currently playing track
    const playbackState = await req.spotifyApi.getMyCurrentPlaybackState();

    if (
      !playbackState.body ||
      !playbackState.body.is_playing ||
      !playbackState.body.item
    ) {
      console.log(`User ${req.userId} is not currently playing anything.`);
      return res.json({ isPlaying: false, features: null });
    }

    const trackId = playbackState.body.item.id;
    console.log(
      `User ${req.userId} is playing track ID: ${trackId}. Fetching audio features...`
    );

    // Then, get the audio features for that track
    const featuresData = await req.spotifyApi.getAudioFeaturesForTrack(trackId);

    if (!featuresData.body) {
      console.error(`Failed to get audio features for track ${trackId}`);
      return res
        .status(500)
        .json({ error: "Failed to retrieve audio features." });
    }

    // Return the relevant audio features
    // Reference: https://developer.spotify.com/documentation/web-api/reference/get-audio-features
    const features = {
      id: featuresData.body.id,
      danceability: featuresData.body.danceability,
      energy: featuresData.body.energy,
      key: featuresData.body.key,
      loudness: featuresData.body.loudness,
      mode: featuresData.body.mode,
      speechiness: featuresData.body.speechiness,
      acousticness: featuresData.body.acousticness,
      instrumentalness: featuresData.body.instrumentalness,
      liveness: featuresData.body.liveness,
      valence: featuresData.body.valence,
      tempo: featuresData.body.tempo,
      time_signature: featuresData.body.time_signature,
    };

    res.json({ isPlaying: true, features: features });
  } catch (err) {
    console.error(`Error fetching audio features for ${req.userId}:`, err);
    if (err.statusCode === 401) {
      // Handle potential token issues not caught by middleware (rare)
      return res
        .status(401)
        .json({ error: "Authentication error. Please log in again." });
    }
    if (err.statusCode === 429) {
      return res
        .status(429)
        .json({ error: "Spotify API rate limit exceeded." });
    }
    res.status(500).json({ error: "Failed to fetch audio features." });
  }
});

module.exports = router;
