require("dotenv").config();
const express = require("express");
const path = require("path");
const session = require("express-session");
const SQLiteStore = require("connect-sqlite3")(session);
const { initializeDatabase } = require("./src/utils/db"); // Import initializeDatabase

// Import routes
const authRoutes = require("./src/routes/auth");
const apiRoutes = require("./src/routes/api"); // Import API routes

const app = express();
const PORT = process.env.PORT || 3000;

// --- Global State (Simple approach for this project) ---
let activeSpotifyUserId = null; // Stores the Spotify ID of the last authenticated user
// Export it for use in controllers (using a simple object wrapper)
const globalState = { activeSpotifyUserId };
module.exports.globalState = globalState;

// --- Database Initialization ---
initializeDatabase()
  .then(() => {
    console.log("Database initialization complete.");

    // --- Session Middleware Setup ---
    if (!process.env.SESSION_SECRET) {
      console.error("FATAL ERROR: SESSION_SECRET is not defined in .env file.");
      process.exit(1); // Exit if session secret is not set
    }
    app.use(
      session({
        store: new SQLiteStore({
          db: process.env.DATABASE_PATH || "visualizer.db", // Use the db filename from .env
          dir: path.join(__dirname, "db"), // Specify the directory for the session db file
          table: "sessions", // Optional: specify table name for sessions
        }),
        secret: process.env.SESSION_SECRET,
        resave: false,
        saveUninitialized: false,
        cookie: {
          maxAge: 1000 * 60 * 60 * 24 * 7, // 1 week
          // secure: process.env.NODE_ENV === 'production' // Use secure cookies in production
        },
      })
    );

    // --- Other Middleware ---
    app.use(express.json());
    app.use(express.urlencoded({ extended: true }));

    // Serve static files from the client build directory (adjust path as needed)
    // app.use(express.static(path.join(__dirname, '../client/dist')));

    // --- Setup Routes ---
    app.use("/auth", authRoutes);
    app.use("/api", apiRoutes); // Mount API routes under /api

    // Basic root route
    app.get("/", (req, res) => {
      res.send("Spotify LED Visualizer Server is running!");
    });

    // --- Error Handling Middleware ---
    app.use((err, req, res, next) => {
      console.error(err.stack);
      res.status(500).send("Something broke!");
    });

    // --- Start Server ---
    app.listen(PORT, () => {
      console.log(`Server listening on port ${PORT}`);
    });
  })
  .catch((err) => {
    console.error("Failed to initialize database:", err);
    process.exit(1); // Exit if database initialization fails
  });
