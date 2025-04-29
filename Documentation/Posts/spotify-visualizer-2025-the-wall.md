# Building a Spotify LED Visualizer in 2025: A Journey from Hope to 403

## Introduction

This semester, I set out to build a real-time LED visualizer for Spotify. The plan: use Astro for the frontend, Supabase for the database, and an Arduino Nano to drive an 8x32 LED panel. The user would scan a QR code, log in with Spotify, and the Arduino would fetch song data to create beautiful, music-driven visuals. I was especially excited to use Spotify's audio features—danceability, energy, tempo, and more—to make the visuals truly reactive to the music.

## The Architecture

- **Frontend:** Astro (no React, just vanilla JS and Astro's built-ins)
- **Database:** Supabase (for storing Spotify tokens)
- **API:** Astro serverless API routes
- **Device:** Arduino Nano polling the API for song data

The user flow:
1. Scan a QR code to log in with Spotify.
2. The backend fetches the user's currently playing song and all the juicy audio features.
3. The Arduino reads from the endpoint and lights up the LEDs in sync with the music.

## The Build

### 1. Astro + Supabase Setup
- Initialized a new Astro project with `pnpm`.
- Added `@supabase/supabase-js`, `spotify-web-api-node`, and `qrcode`.
- Set up environment variables for Supabase and Spotify credentials.
- Created a Supabase table `spotify_tokens` to store user tokens.

### 2. Spotify OAuth Flow
- Built `/api/auth/login` to redirect users to Spotify's login page.
- Built `/api/auth/callback` to handle the OAuth callback, exchange the code for tokens, and store them in Supabase.
- Debugged a ton of issues with Astro's API route context and prerendering (pro tip: add `export const prerender = false;` to your API routes!).
- Finally got the OAuth flow working—tokens were being saved, and the user was redirected home.

### 3. Device Data Endpoint
- Built `/api/device/data` to:
  - Fetch the latest Spotify tokens from Supabase.
  - Refresh tokens if expired.
  - Fetch the user's current playback info.
  - Fetch audio features for the current track.
- The Arduino would poll this endpoint to get real-time song data.

## The Wall: 403 Forbidden

Everything was working—except the most important part. Every call to Spotify's `/audio-features/{id}` endpoint returned a 403 Forbidden error. It didn't matter what track I tried (Billie Eilish, global hits, you name it), or how many times I re-authenticated, re-added my account as a tester, or even created a new app.

I tried:
- Verifying all scopes and permissions.
- Manually calling the endpoint with a fresh user access token.
- Using the Spotify Web API Console.
- Re-creating the app, re-adding testers, and re-authenticating.

**Nothing worked.**

## The Realization: Spotify Deprecated Audio Features

After hours of debugging, I discovered the truth: **Spotify has deprecated the audio features endpoint for new apps.**

> The `/v1/audio-features/{id}` endpoint is no longer available for new apps or may be restricted for all apps. This is not a bug in your code, your scopes, or your app setup—Spotify is intentionally removing access to this data.

This means:
- No more danceability, energy, valence, tempo, or any of the cool audio features that made Spotify visualizers awesome.
- Even the Spotify Web API Console returns 403 for these endpoints.

## What You Can Still Do

You can still access playback info:
- Track name, artist, album
- Progress in the current track
- Album art
- Playback state (playing/paused)
- Device info (name, type, volume)
- Shuffle/repeat state

But you **cannot** access:
- Audio features (danceability, energy, tempo, etc.)
- Audio analysis (beats, sections, etc.)

## Conclusion: The Immovable Freakin' Object

I hit an immovable freakin' object. Spotify has deprecated the most interesting part of their API for new apps. If you're building a visualizer in 2025, you can still make something cool with playback data, but you won't get the deep, music-driven reactivity that audio features enabled.

**Lesson learned:** Always check the API changelog before you start a project. And if you want to build a truly reactive Spotify visualizer, you'll need to get creative—or hope Spotify brings back audio features for developers someday.

---

*If you're reading this and have a workaround, please let me know. Until then, I'll be over here, making the prettiest progress bar you've ever seen.*
