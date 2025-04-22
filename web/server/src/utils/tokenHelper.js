const db = require("./db");
const SpotifyWebApi = require("spotify-web-api-node");

// Re-initialize a SpotifyWebApi instance (needed because the global one might have wrong tokens)
// This ensures we use the correct credentials for refreshing
const spotifyApiRefresh = new SpotifyWebApi({
  clientId: process.env.SPOTIFY_CLIENT_ID,
  clientSecret: process.env.SPOTIFY_CLIENT_SECRET,
  redirectUri: process.env.SPOTIFY_REDIRECT_URI,
});

/**
 * Fetches tokens for a user, refreshes if necessary, and returns a configured Spotify API client.
 * @param {string} userId - The Spotify user ID.
 * @returns {Promise<SpotifyWebApi|null>} A configured SpotifyWebApi instance with valid tokens, or null if tokens are not found or refresh fails.
 */
async function getAuthenticatedSpotifyApi(userId) {
  if (!userId) {
    console.error("getAuthenticatedSpotifyApi called without userId");
    return null;
  }

  try {
    const tokenInfo = await db.getTokens(userId);

    if (!tokenInfo) {
      console.error(`No tokens found in DB for user: ${userId}`);
      return null;
    }

    let { access_token, refresh_token, expires_at } = tokenInfo;

    // Check if the token is expired or close to expiring (e.g., within 60 seconds)
    const bufferSeconds = 60;
    if (Date.now() >= expires_at - bufferSeconds * 1000) {
      console.log(
        `Token for user ${userId} expired or expiring soon. Refreshing...`
      );

      // Use the separate instance for refreshing
      spotifyApiRefresh.setRefreshToken(refresh_token);

      try {
        const data = await spotifyApiRefresh.refreshAccessToken();
        access_token = data.body["access_token"];
        const newExpiresIn = data.body["expires_in"]; // Seconds
        expires_at = Date.now() + newExpiresIn * 1000;

        // Note: Spotify might issue a new refresh token during refresh
        // It's good practice to save it if provided
        const new_refresh_token = data.body["refresh_token"];
        if (new_refresh_token) {
          refresh_token = new_refresh_token;
        }

        // Save the updated tokens back to the database
        await db.saveTokens(userId, access_token, refresh_token, expires_at);
        console.log(`Token refreshed and saved for user: ${userId}`);
      } catch (refreshError) {
        console.error(
          `Failed to refresh token for user ${userId}:`,
          refreshError
        );
        // Handle specific errors, e.g., invalid refresh token might require re-authentication
        if (refreshError.body && refreshError.body.error === "invalid_grant") {
          console.error(
            `Invalid refresh token for user ${userId}. User needs to re-authenticate.`
          );
          // Optionally, delete the invalid tokens from DB or mark user as needing re-auth
        }
        return null; // Refresh failed
      }
    }

    // Return a NEW SpotifyWebApi instance configured with the valid tokens
    // This avoids conflicts if multiple users are handled concurrently
    const userSpotifyApi = new SpotifyWebApi({
      clientId: process.env.SPOTIFY_CLIENT_ID,
      clientSecret: process.env.SPOTIFY_CLIENT_SECRET,
      redirectUri: process.env.SPOTIFY_REDIRECT_URI,
      accessToken: access_token,
      refreshToken: refresh_token, // Include refresh token for completeness, though not strictly needed for API calls
    });

    return userSpotifyApi;
  } catch (error) {
    console.error(
      `Error in getAuthenticatedSpotifyApi for user ${userId}:`,
      error
    );
    return null;
  }
}

module.exports = { getAuthenticatedSpotifyApi };
