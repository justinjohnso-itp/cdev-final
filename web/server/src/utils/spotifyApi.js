import SpotifyWebApi from "spotify-web-api-node"; // Changed import
import "dotenv/config"; // Use config() directly

// Function to create a new Spotify API client instance
export function createSpotifyApiClient() {
  // Changed to export
  return new SpotifyWebApi({
    clientId: process.env.SPOTIFY_CLIENT_ID,
    clientSecret: process.env.SPOTIFY_CLIENT_SECRET,
    redirectUri: process.env.SPOTIFY_REDIRECT_URI,
  });
}

// Removed module.exports
