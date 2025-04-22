import { createClient } from "@supabase/supabase-js";
import "dotenv/config";

// Check for required environment variables
if (!process.env.SUPABASE_URL || !process.env.SUPABASE_ANON_KEY) {
  console.error(
    "FATAL ERROR: SUPABASE_URL or SUPABASE_ANON_KEY is not defined in .env file."
  );
  process.exit(1);
}

// Create a single supabase client for interacting with your database
const supabase = createClient(
  process.env.SUPABASE_URL,
  process.env.SUPABASE_ANON_KEY
);

let initializationComplete = false;

export async function initializeDatabase() {
  if (initializationComplete) {
    return supabase; // Return the existing client
  }

  try {
    console.log(`Connecting to Supabase project: ${process.env.SUPABASE_URL}`);

    // Check connection by making a simple query (optional but good practice)
    const { error: connectionError } = await supabase
      .from("spotify_tokens")
      .select("user_id")
      .limit(1);
    if (connectionError && connectionError.code !== "42P01") {
      // Ignore "table does not exist" error initially
      throw connectionError;
    }
    console.log("Supabase connection successful (or table doesn't exist yet).");

    // Create the tokens table if it doesn't exist (PostgreSQL syntax)
    // Using TIMESTAMPTZ for updated_at
    // Supabase handles table creation via its UI or migrations, but we can ensure it here.
    // Note: Supabase client doesn't directly execute raw SQL easily for table creation.
    // It's often better to create tables via the Supabase UI or SQL editor.
    // This code assumes the table *might* exist.
    console.log(
      "Ensure 'spotify_tokens' table exists in your Supabase project."
    );
    console.log(
      "Columns: user_id (text, primary key), access_token (text), refresh_token (text), expires_at (bigint), updated_at (timestamptz)"
    );

    // We also need the sessions table for connect-pg-simple
    // Default schema: CREATE TABLE "session" ("sid" varchar NOT NULL COLLATE "default","sess" json NOT NULL,"expire" timestamp(6) NOT NULL) WITH (OIDS=FALSE);
    // ALTER TABLE "session" ADD CONSTRAINT "session_pkey" PRIMARY KEY ("sid") NOT DEFERRABLE INITIALLY IMMEDIATE;
    // CREATE INDEX "IDX_session_expire" ON "session" ("expire");
    console.log("Ensure 'session' table exists (for connect-pg-simple).");

    initializationComplete = true;
    return supabase;
  } catch (err) {
    console.error("Error initializing Supabase connection:", err);
    throw err; // Re-throw error to indicate failure
  }
}

export async function getDb() {
  if (!initializationComplete) {
    return await initializeDatabase();
  }
  return supabase;
}

// Function to save or update tokens using Supabase client
export async function saveTokens(userId, accessToken, refreshToken, expiresAt) {
  const supabaseClient = await getDb();
  try {
    const { data, error } = await supabaseClient.from("spotify_tokens").upsert(
      {
        user_id: userId,
        access_token: accessToken,
        refresh_token: refreshToken,
        expires_at: expiresAt,
        updated_at: new Date().toISOString(), // Use current timestamp
      },
      { onConflict: "user_id" }
    ); // Specify the conflict column for upsert

    if (error) throw error;
    console.log(`Tokens saved/updated for user: ${userId}`);
  } catch (err) {
    console.error("Error saving tokens to Supabase:", err);
  }
}

// Function to retrieve tokens using Supabase client
export async function getTokens(userId) {
  const supabaseClient = await getDb();
  try {
    const { data, error } = await supabaseClient
      .from("spotify_tokens")
      .select("access_token, refresh_token, expires_at")
      .eq("user_id", userId)
      .single(); // Use single() to get one row or null

    if (error && error.code !== "PGRST116") {
      // Ignore 'PGRST116' (Row not found)
      throw error;
    }
    return data; // Returns the row object or null
  } catch (err) {
    console.error("Error retrieving tokens from Supabase:", err);
    return null;
  }
}

// No explicit close function needed for the Supabase client like a pool
