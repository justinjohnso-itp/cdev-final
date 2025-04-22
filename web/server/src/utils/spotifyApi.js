const SpotifyWebApi = require("spotify-web-api-node");
require("dotenv").config();

// Function to create a new Spotify API client instance
function createSpotifyApiClient() {
  return new SpotifyWebApi({
    clientId: process.env.SPOTIFY_CLIENT_ID,
    clientSecret: process.env.SPOTIFY_CLIENT_SECRET,
    redirectUri: process.env.SPOTIFY_REDIRECT_URI,
  });
}

module.exports = { createSpotifyApiClient };
