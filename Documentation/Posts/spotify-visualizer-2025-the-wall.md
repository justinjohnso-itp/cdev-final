# Spotify LED Visualizer Dev Log 2: The API Wall

## Context

This is a follow-up to my previous post ([see here](https://justin-itp.notion.site/Final-The-joys-of-the-Spotify-API-1dd9127f465d804b8ac9fb7e471e239a?pvs=4)). If you haven’t read that, you’ll want to start there for the full background and initial architecture.

## Progress Recap

- Astro frontend, Supabase for token storage, Arduino Nano for the LED panel.
- User scans a QR code, logs in with Spotify, Arduino fetches song data from a custom API.
- The plan was to use Spotify’s audio features (danceability, energy, tempo, etc.) to drive the visuals in a meaningful way.

## The Build (What Actually Worked)

- Astro project and Supabase integration: straightforward.
- Spotify OAuth flow: after a lot of debugging (Astro API quirks, prerendering issues), I got token storage and refresh working.
- `/api/device/data` endpoint: fetches playback info and (supposedly) audio features for the current track.

## The Wall: 403 on Audio Features

Here’s where things fell apart. Every call to `/audio-features/{id}` returned a 403 Forbidden. It didn’t matter what track I tried, how many times I re-authenticated, or how many times I recreated the app and re-added myself as a tester. I tried everything:

- Double-checked scopes and permissions.
- Manually called the endpoint with a fresh user access token.
- Used the Spotify Web API Console (same 403).
- Removed and re-added my account as a tester.
- Created a new app from scratch.

Nothing worked. Every single attempt to get audio features was met with a 403. No error message, no workaround, just a hard stop.

## The Realization: Spotify Deprecated the Good Stuff

After digging through the docs and changelogs, I found the answer: **Spotify has deprecated the audio features endpoint for new apps.**

- `/v1/audio-features/{id}` is no longer available for new apps (and possibly all apps soon).
- This isn’t a bug, a scope issue, or a misconfiguration. It’s just gone.
- Even the official Spotify Web API Console returns 403 for these endpoints.

## What’s Left?

You can still get basic playback info:
- Track name, artist, album
- Progress in the current track
- Album art
- Playback state (playing/paused)
- Device info (name, type, volume)
- Shuffle/repeat state

But you **cannot** get:
- Audio features (danceability, energy, tempo, etc.)
- Audio analysis (beats, sections, etc.)

## Where This Leaves the Project

The core infrastructure works. I can show what’s playing, progress, and some basic visuals. But the entire point—the thing that would have made this visualizer interesting—was the audio features. That’s just not possible anymore. There’s no workaround, no clever hack, no amount of OAuth debugging that will bring it back.

## Final Thoughts

If you’re reading this and thinking about building something similar: check the API changelog first. If you want to do anything interesting with Spotify data, you’re out of luck. The most compelling part of the API is simply gone. For now, this project is just a progress bar with album art.

If Spotify ever brings back audio features, maybe I’ll revisit this. But for now, that’s the end of the road.
