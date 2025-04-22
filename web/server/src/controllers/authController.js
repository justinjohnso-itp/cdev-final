const spotifyApi = require("../utils/spotifyApi");
const querystring = require("querystring");
const db = require("../utils/db"); // Import database utility
const { globalState } = require("../../index"); // Import global state

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

// Remove in-memory storage, will use DB now
// let accessToken = null;
// let refreshToken = null;
// let expiresAt = null;

exports.login = (req, res) => {
  // Generate a random state string for security
  const state = Math.random().toString(36).substring(2, 15);
  // Store state in session
  req.session.spotifyAuthState = state;
  console.log("Stored state in session:", req.session.spotifyAuthState);

  const authorizeURL = spotifyApi.createAuthorizeURL(scopes, state);
  console.log("Redirecting to Spotify for authorization:", authorizeURL);
  res.redirect(authorizeURL);
};

exports.callback = async (req, res) => {
  const { code, state, error } = req.query;
  const storedState = req.session.spotifyAuthState;

  console.log("Received state:", state);
  console.log("Stored state:", storedState);

  // Clear the state from session immediately after retrieving it
  delete req.session.spotifyAuthState;

  // Verify the state parameter
  if (!state || state !== storedState) {
    console.error("State mismatch error");
    res.status(400).send("Error: State mismatch");
    return;
  }

  if (error) {
    console.error("Callback Error:", error);
    res.status(400).send(`Callback Error: ${error}`);
    return;
  }

  if (!code) {
    console.error("No code received in callback");
    res.status(400).send("Error: No code received");
    return;
  }

  console.log("Authorization code received:", code);

  try {
    // Exchange code for tokens
    const data = await spotifyApi.authorizationCodeGrant(code);
    const accessToken = data.body["access_token"];
    const refreshToken = data.body["refresh_token"];
    const expiresIn = data.body["expires_in"]; // Seconds
    const expiresAt = Date.now() + expiresIn * 1000; // Calculate expiration time in ms

    // Set tokens on the API object for immediate use
    spotifyApi.setAccessToken(accessToken);
    spotifyApi.setRefreshToken(refreshToken);

    console.log("Access Token obtained."); // Don't log the token itself
    // console.log('Refresh Token:', refreshToken); // Avoid logging refresh token
    console.log(`Access token expires in ${expiresIn} seconds.`);

    // Get user profile information (specifically the user ID)
    const userData = await spotifyApi.getMe();
    const userId = userData.body.id;
    const userDisplayName = userData.body.display_name;
    console.log("Fetched user info:", { userId, userDisplayName });

    // Save tokens to the database
    await db.saveTokens(userId, accessToken, refreshToken, expiresAt);

    // Store user ID in session to mark as logged in
    req.session.userId = userId;
    console.log(`User ${userId} logged in and session set.`);

    // *** Update the active user ID in global state ***
    globalState.activeSpotifyUserId = userId;
    console.log(
      `Set activeSpotifyUserId to: ${globalState.activeSpotifyUserId}`
    );

    // Redirect to the frontend application
    // TODO: Make the redirect URL configurable (e.g., via .env)
    const clientUrl = process.env.CLIENT_URL || "http://localhost:4321"; // Default to Astro dev port
    res.redirect(clientUrl);
  } catch (err) {
    console.error("Error during Spotify callback processing:", err);
    if (err.body) {
      console.error("Spotify API Error Body:", err.body);
    }
    if (err.headers) {
      console.error("Spotify API Error Headers:", err.headers);
    }
    if (err.statusCode) {
      console.error("Spotify API Error Status Code:", err.statusCode);
    }
    res.status(500).send(`Error processing Spotify callback: ${err.message}`);
  }
};

// TODO: Implement refreshToken function using DB
// exports.refreshToken = async (req, res) => { ... };

// TODO: Re-evaluate getAccessToken function - might not be needed if API calls
// are made in context of a logged-in user where tokens are fetched from DB/refreshed.
// exports.getAccessToken = async () => {
//   // This needs logic to get current user ID (from session?), fetch tokens from DB,
//   // check expiration, and potentially refresh.
//   console.warn('getAccessToken function needs implementation based on user context');
//   return null;
// };
