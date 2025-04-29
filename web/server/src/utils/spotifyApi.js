import SpotifyWebApi from "spotify-web-api-node";
import "dotenv/config";

// Determine the redirect URI based on the environment
const isProduction = process.env.NODE_ENV === "production";
const localRedirectPath = "/auth/callback"; // Path for the callback
const localServerBase = `http://127.0.0.1:${process.env.PORT || 3000}`;
const deployedServerBase = process.env.DEPLOYED_SERVER_URL; // Make sure this is set in .env

// Construct the redirect URI
let redirectUri;
if (isProduction) {
  if (!deployedServerBase) {
    console.error(
      "FATAL ERROR: NODE_ENV is production, but DEPLOYED_SERVER_URL is not set in .env."
    );
    process.exit(1); // Exit if deployed URL is missing in production
  }
  redirectUri = `${deployedServerBase}${localRedirectPath}`;
} else {
  // For development, ensure SPOTIFY_REDIRECT_URI is set in .env or default to localhost
  // Using localhost is generally preferred and allowed by Spotify for dev
  redirectUri =
    process.env.SPOTIFY_REDIRECT_URI ||
    `${localServerBase}${localRedirectPath}`;
  if (!process.env.SPOTIFY_REDIRECT_URI) {
    console.log(`SPOTIFY_REDIRECT_URI not set, defaulting to ${redirectUri}`);
  }
}

console.log(`Using Spotify Redirect URI: ${redirectUri}`);

// Function to create a new Spotify API client instance
export function createSpotifyApiClient() {
  if (!process.env.SPOTIFY_CLIENT_ID || !process.env.SPOTIFY_CLIENT_SECRET) {
    console.error(
      "FATAL ERROR: SPOTIFY_CLIENT_ID or SPOTIFY_CLIENT_SECRET is not set in .env."
    );
    process.exit(1);
  }
  return new SpotifyWebApi({
    clientId: process.env.SPOTIFY_CLIENT_ID,
    clientSecret: process.env.SPOTIFY_CLIENT_SECRET,
    redirectUri: redirectUri, // Use the dynamically determined URI
  });
}
