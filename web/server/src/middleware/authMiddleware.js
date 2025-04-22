const { getAuthenticatedSpotifyApi } = require("../utils/tokenHelper"); // Import the new helper

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
    // Use the helper function to get a configured API client
    const authenticatedApi = await getAuthenticatedSpotifyApi(userId);

    if (!authenticatedApi) {
      // Helper function handles logging errors. If it returns null,
      // it means tokens weren't found or refresh failed.
      console.log(
        `Auth Middleware: Failed to get authenticated API client for user ${userId}. Forcing re-login.`
      );
      // Clear potentially invalid session
      req.session.destroy((err) => {
        if (err) console.error("Error destroying session:", err);
      });
      return res
        .status(401)
        .json({
          error:
            "Authentication invalid or session expired. Please log in again.",
        });
    }

    // Attach the authenticated client and user ID to the request object
    req.spotifyApi = authenticatedApi;
    req.userId = userId;

    console.log(`Auth Middleware: User ${userId} authenticated. Proceeding.`);
    next(); // Proceed to the next middleware or route handler
  } catch (err) {
    // Catch any unexpected errors during the process
    console.error("Auth Middleware: Unexpected error:", err);
    return res
      .status(500)
      .json({ error: "Internal server error during authentication check." });
  }
};

module.exports = { requireAuth };
