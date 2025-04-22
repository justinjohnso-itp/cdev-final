const db = require("../utils/db");
const { createSpotifyApiClient } = require("../utils/spotifyApi");

// Middleware to ensure user is authenticated and tokens are valid/refreshed
const requireAuth = async (req, res, next) => {
  const userId = req.session.userId;

  if (!userId) {
    console.log("Auth Middleware: No userId in session.");
    return res
      .status(401)
      .json({ error: "Authentication required. Please log in." });
  }

  try {
    let tokens = await db.getTokens(userId);

    if (!tokens) {
      console.log(
        `Auth Middleware: No tokens found in DB for userId: ${userId}`
      );
      // Clear potentially invalid session
      req.session.destroy((err) => {
        if (err) console.error("Error destroying session:", err);
      });
      return res
        .status(401)
        .json({ error: "Authentication invalid. Please log in again." });
    }

    const now = Date.now();
    // Check if token is expired or will expire in the next 60 seconds
    if (tokens.expires_at <= now + 60000) {
      console.log(
        `Auth Middleware: Access token for ${userId} expired or expiring soon. Refreshing...`
      );

      // Create a temporary client just for refreshing
      const refreshClient = createSpotifyApiClient();
      refreshClient.setRefreshToken(tokens.refresh_token);

      try {
        const data = await refreshClient.refreshAccessToken();
        const newAccessToken = data.body["access_token"];
        const newExpiresIn = data.body["expires_in"]; // Seconds
        const newExpiresAt = Date.now() + newExpiresIn * 1000;
        // Note: Spotify might sometimes return a new refresh token, but often doesn't.
        // If it does, we should save it. For now, assume the old one is still valid.
        const newRefreshToken =
          data.body["refresh_token"] || tokens.refresh_token;

        console.log(
          `Auth Middleware: Token refreshed for ${userId}. New expiry: ${new Date(
            newExpiresAt
          ).toISOString()}`
        );

        // Update database with new token info
        await db.saveTokens(
          userId,
          newAccessToken,
          newRefreshToken,
          newExpiresAt
        );

        // Update the tokens object for the current request
        tokens.access_token = newAccessToken;
        tokens.refresh_token = newRefreshToken; // Update in case it changed
        tokens.expires_at = newExpiresAt;
      } catch (refreshError) {
        console.error(
          `Auth Middleware: Failed to refresh token for ${userId}:`,
          refreshError
        );
        // If refresh fails (e.g., refresh token revoked), force re-login
        req.session.destroy((err) => {
          if (err)
            console.error(
              "Error destroying session after refresh failure:",
              err
            );
        });
        return res
          .status(401)
          .json({ error: "Failed to refresh session. Please log in again." });
      }
    }

    // Create an authenticated Spotify API client for this request
    const authenticatedApi = createSpotifyApiClient();
    authenticatedApi.setAccessToken(tokens.access_token);
    authenticatedApi.setRefreshToken(tokens.refresh_token); // Set refresh token for potential future use within the same request lifecycle if needed

    // Attach the authenticated client and user ID to the request object
    req.spotifyApi = authenticatedApi;
    req.userId = userId;

    console.log(`Auth Middleware: User ${userId} authenticated. Proceeding.`);
    next(); // Proceed to the next middleware or route handler
  } catch (err) {
    console.error("Auth Middleware: Error during token retrieval/check:", err);
    return res
      .status(500)
      .json({ error: "Internal server error during authentication check." });
  }
};

module.exports = { requireAuth };
