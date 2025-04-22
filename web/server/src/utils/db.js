const sqlite3 = require("sqlite3").verbose();
const { open } = require("sqlite");
require("dotenv").config();

const dbPath = process.env.DATABASE_PATH || "./db/visualizer.db";
let db = null;

async function initializeDatabase() {
  if (db) {
    return db;
  }

  try {
    console.log(`Opening database at: ${dbPath}`);
    db = await open({
      filename: dbPath,
      driver: sqlite3.Database,
    });

    console.log("Database connected.");

    // Create the tokens table if it doesn't exist
    await db.exec(`
      CREATE TABLE IF NOT EXISTS spotify_tokens (
        user_id TEXT PRIMARY KEY,
        access_token TEXT NOT NULL,
        refresh_token TEXT NOT NULL,
        expires_at INTEGER NOT NULL,
        updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
      );
    `);
    console.log("Checked/created spotify_tokens table.");

    return db;
  } catch (err) {
    console.error("Error initializing database:", err);
    throw err; // Re-throw error to indicate failure
  }
}

async function getDb() {
  if (!db) {
    return await initializeDatabase();
  }
  return db;
}

// Function to save or update tokens
async function saveTokens(userId, accessToken, refreshToken, expiresAt) {
  const dbInstance = await getDb();
  const sql = `
    INSERT INTO spotify_tokens (user_id, access_token, refresh_token, expires_at)
    VALUES (?, ?, ?, ?)
    ON CONFLICT(user_id) DO UPDATE SET
      access_token = excluded.access_token,
      refresh_token = excluded.refresh_token,
      expires_at = excluded.expires_at,
      updated_at = CURRENT_TIMESTAMP;
  `;
  try {
    await dbInstance.run(sql, userId, accessToken, refreshToken, expiresAt);
    console.log(`Tokens saved/updated for user: ${userId}`);
  } catch (err) {
    console.error("Error saving tokens:", err);
  }
}

// Function to retrieve tokens
async function getTokens(userId) {
  const dbInstance = await getDb();
  const sql = `SELECT access_token, refresh_token, expires_at FROM spotify_tokens WHERE user_id = ?`;
  try {
    const row = await dbInstance.get(sql, userId);
    return row;
  } catch (err) {
    console.error("Error retrieving tokens:", err);
    return null;
  }
}

module.exports = {
  initializeDatabase,
  getDb,
  saveTokens,
  getTokens,
};
