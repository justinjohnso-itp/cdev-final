# Spotify LED Visualizer

This project displays the currently playing Spotify track name scrolling across an 8x32 WS2812B LED matrix, colored with a gradient derived from the track's album art. It uses an Arduino Nano connected to the LED matrix and a web application (built with Astro and hosted on Netlify) to handle Spotify authentication and data fetching.

## Components

1.  **Arduino (`/arduino`)**:
    *   Controls the 8x32 LED matrix (Adafruit NeoMatrix/GFX libraries).
    *   Connects to WiFi.
    *   Polls the web server API endpoint for current playback data.
    *   Displays scrolling track name with a color gradient based on the album art palette.
2.  **Web Application (`/web/client` & `/web/server`)**:
    *   Built with Astro.
    *   Handles Spotify OAuth 2.0 authentication flow.
    *   Uses Supabase for secure token storage (though this might be simplified in current implementation).
    *   Provides an API endpoint (`/api/device/data`) that the Arduino polls.
    *   Fetches current playback state, track info, and album art palette from Spotify API.
    *   Uses `node-colorthief` to extract the color palette from the album art image URL.

## Features

*   Displays currently playing Spotify track name.
*   Text scrolls smoothly across the 8x32 LED matrix.
*   Text color is a dynamic gradient interpolated from the album art's color palette.
*   Handles play/pause state changes.
*   Gracefully handles WiFi disconnection and reconnection attempts.

## Setup

### Arduino

1.  **Hardware**: Connect the 8x32 WS2812B matrix (DIN) to the specified pin on the Arduino Nano (default: Pin 2). Ensure proper power supply for the matrix.
2.  **Secrets**: Create `arduino/include/secrets.h` with your WiFi credentials:
    ```c++
    #define WIFI_SSID "YOUR_WIFI_SSID"
    #define WIFI_PASS "YOUR_WIFI_PASSWORD"
    ```
3.  **PlatformIO**: Open the `/arduino` folder in VS Code with PlatformIO installed. Build and upload the code to the Arduino Nano.

### Web Application

1.  **Spotify App**: Create a Spotify Developer application to get your Client ID and Client Secret. Set the Redirect URI to `YOUR_DEPLOYED_URL/api/auth/callback` (e.g., `https://your-app.netlify.app/api/auth/callback`) or `http://localhost:4321/api/auth/callback` for local development.
2.  **Environment Variables**: Create a `.env` file in the `/web/client` directory with your Spotify credentials:
    ```env
    SPOTIFY_CLIENT_ID=YOUR_SPOTIFY_CLIENT_ID
    SPOTIFY_CLIENT_SECRET=YOUR_SPOTIFY_CLIENT_SECRET
    # Optional: If using Supabase for token storage
    # SUPABASE_URL=YOUR_SUPABASE_URL
    # SUPABASE_KEY=YOUR_SUPABASE_ANON_KEY
    # Required: Base URL for redirects
    PUBLIC_BASE_URL=http://localhost:4321 # Or your deployed URL
    ```
3.  **Dependencies**: Navigate to `/web/client` and run `pnpm install`.
4.  **Development**: Run `pnpm dev` to start the local development server.
5.  **Deployment**: Deploy the `/web/client` directory to a hosting provider like Netlify. Ensure environment variables are set in the deployment environment.

## Usage

1.  Power on the Arduino. It will connect to WiFi and start polling the server.
2.  Navigate to the web application's URL.
3.  Click the "Login with Spotify" button and authorize the application.
4.  Play music on Spotify. The track name should start scrolling on the LED matrix.

## Development Notes

*   **API Pivot**: The original goal was to create audio-reactive visualizations based on Spotify's audio features (tempo, energy, etc.). However, Spotify deprecated access to these endpoints (`/v1/audio-features`, `/v1/audio-analysis`) for newer applications. The project pivoted to focus on displaying the track name with colors derived from the album art palette, which is still accessible. See `/Documentation/Posts/spotify-visualizer-2025-the-wall.md` for more details.
*   **Animation Stutter**: Due to the blocking nature of network requests on the Arduino Nano, a slight pause or stutter in the scrolling animation may occur when the device fetches data from the server (currently every 30 seconds).
