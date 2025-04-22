const spotifyApi = require("../utils/spotifyApi");
const querystring = require("querystring");

// Define the scopes required by the application
// Reference: https://developer.spotify.com/documentation/web-api/concepts/scopes
const scopes = [
  "ugc-image-upload",
  "user-read-playback-state",
  "user-modify-playback-state",
  "user-read-currently-playing",
  "streaming",
  "app-remote-control",
  "user-read-email",
  "user-read-private",
  "playlist-read-collaborative",
  "playlist-modify-public",
  "playlist-read-private",
  "playlist-modify-private",
  "user-library-modify",
  "user-library-read",
  "user-top-read",
  "user-read-playback-position",
  "user-read-recently-played",
  "user-follow-read",
  "user-follow-modify",
];

// Simple in-memory storage for tokens (replace with DB later)
let accessToken = null;
let refreshToken = null;
let expiresAt = null;

exports.login = (req, res) => {
  // Generate a random state string for security
  const state = Math.random().toString(36).substring(2, 15);
  // TODO: Store state temporarily (e.g., in session or DB) to verify in callback

  const authorizeURL = spotifyApi.createAuthorizeURL(scopes, state);
  console.log("Redirecting to Spotify for authorization:", authorizeURL);
  res.redirect(authorizeURL);
};

exports.callback = async (req, res) => {
  const { code, state, error } = req.query;

  // TODO: Verify the state parameter matches the one generated in /login

  if (error) {
    console.error("Callback Error:", error);
    res.send(`Callback Error: ${error}`);
    return;
  }

  if (!code) {
    console.error("No code received in callback");
    res.send("Error: No code received");
    return;
  }

  console.log("Authorization code received:", code);

  try {
    const data = await spotifyApi.authorizationCodeGrant(code);
    accessToken = data.body["access_token"];
    refreshToken = data.body["refresh_token"];
    const expiresIn = data.body["expires_in"]; // Seconds
    expiresAt = Date.now() + expiresIn * 1000;

    spotifyApi.setAccessToken(accessToken);
    spotifyApi.setRefreshToken(refreshToken);

    console.log("Access Token:", accessToken);
    console.log("Refresh Token:", refreshToken);
    console.log(`Access token expires in ${expiresIn} seconds.`);

    // Redirect to the frontend, perhaps passing status or user info
    // For now, just send a success message
    res.send("Success! You are authenticated. You can close this window.");

    // TODO: Store tokens securely (DB)
    // TODO: Redirect user to a logged-in page on the frontend
  } catch (err) {
    console.error("Error getting Tokens:", err.message);
    console.error("Error details:", err.body);
    res.send(`Error getting tokens: ${err.message}`);
  }
};

// TODO: Implement refreshToken function
// exports.refreshToken = async (req, res) => { ... };

// Utility function to get the current access token (needs improvement for expiration)
exports.getAccessToken = () => {
  // TODO: Check if token is expired and refresh if needed
  if (!accessToken || Date.now() >= expiresAt) {
    console.log("Access token expired or not set.");
    // Need to implement refresh logic here
    return null;
  }
  return accessToken;
};
