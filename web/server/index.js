import "dotenv/config"; // Use config() directly
import express from "express";
import path from "path";
import { fileURLToPath } from "url"; // For __dirname equivalent
import session from "express-session";
import pg from "pg"; // Import pg pool
import connectPgSimple from "connect-pg-simple"; // Import PostgreSQL session store
import { initializeDatabase } from "./src/utils/db.js"; // Added .js extension, removed getDb as it's not needed here

// Import routes
import authRoutes from "./src/routes/auth.js"; // Added .js extension
import apiRoutes from "./src/routes/api.js"; // Added .js extension

// ESM equivalent for __dirname
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const app = express();
const PORT = process.env.PORT || 3000;

// --- Global State (Simple approach for this project) ---
let activeSpotifyUserId = null; // Stores the Spotify ID of the last authenticated user
// Export it for use in controllers (using a simple object wrapper)
export const globalState = { activeSpotifyUserId }; // Changed to export
// Removed module.exports

// --- Database Initialization ---
initializeDatabase()
  .then(async (supabaseClient) => {
    // Receive the Supabase client
    console.log("Supabase initialization complete.");

    // --- Session Middleware Setup ---
    if (!process.env.SESSION_SECRET) {
      console.error("FATAL ERROR: SESSION_SECRET is not defined in .env file.");
      process.exit(1);
    }
    if (!process.env.DATABASE_URL) {
      console.error(
        "FATAL ERROR: DATABASE_URL (PostgreSQL connection string) is not defined in .env file."
      );
      process.exit(1);
    }

    // Configure PostgreSQL session store
    const PgSession = connectPgSimple(session); // Pass session constructor
    const pgPool = new pg.Pool({
      connectionString: process.env.DATABASE_URL,
      // Add SSL config if required by Supabase/Postgres provider
      // ssl: { rejectUnauthorized: false } // Example, adjust as needed
    });

    app.use(
      session({
        store: new PgSession({
          pool: pgPool, // Connection pool
          tableName: "session", // Use the default table name
          // Insert interval is optional, default is 30 seconds
        }),
        secret: process.env.SESSION_SECRET,
        resave: false,
        saveUninitialized: false,
        cookie: {
          maxAge: 1000 * 60 * 60 * 24 * 7, // 1 week (should match store expiration)
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
    console.error("Failed to initialize database connection:", err);
    process.exit(1); // Exit if database initialization fails
  });
