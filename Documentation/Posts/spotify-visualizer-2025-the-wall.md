# Spotify LED Visualizer Dev Log 2: Adapting to API Changes

This post details the development process following the initial setup ([see Dev Log 1](https://justin-itp.notion.site/Final-The-joys-of-the-Spotify-API-1dd9127f465d804b8ac9fb7e471e239a?pvs=4)), focusing on the challenges encountered with the Spotify API and the subsequent pivot in project goals. The initial plan involved using Spotify's audio features (danceability, energy, tempo) fetched via a custom `/api/device/data` endpoint to drive LED visualizations on an Arduino Nano.

## Encountering the API Limitation

While the OAuth flow and basic data fetching (track name, artist, etc.) were humming along nicely, attempts to retrieve the *actually interesting* audio features using the `/v1/audio-features/{id}` endpoint consistently slammed into a `403 Forbidden` wall. Cue the debugging montage:

- Scopes and permissions? Checked, double-checked, triple-checked. All good.
- Manual API calls with fresh tokens? Same 403 error. Lovely.
- The official Spotify Web API Console? Even *that* returned 403 for this endpoint. Helpful.
- Maybe recreating the Spotify application or re-adding tester accounts? Nope, still 403.

## API Deprecation Confirmed

After exhausting the usual suspects, digging into Spotify's API documentation and developer forums revealed the grim truth: the `/v1/audio-features/{id}` and `/v1/audio-analysis/{id}` endpoints have been deprecated and are simply no longer accessible for newer applications. This wasn't a bug or a misconfiguration; it was a deliberate change by Spotify, effectively pulling the rug out from under the original project concept.

So, what *can* we still get?
- Basic track info: name, artist, album
- Playback progress
- Album art palette (thank goodness for small favors)
- Play/pause state
- Device info
- Shuffle/repeat state

And what's gone forever (for us)?
- The good stuff: Audio features (danceability, energy, tempo, etc.)
- The *really* good stuff: Audio analysis (beats, sections, etc.)

## Project Pivot

The deprecation of audio features meant the core goal of dynamic, audio-reactive visuals was dead on arrival. Time for Plan B. The focus shifted to making the most of the remaining data—primarily the track name and album art color palette—to create the best possible display on the 8x32 LED matrix.

## Revised Goal: Scrolling Text with Palette Colors

So, the original vision hit a roadblock with the Spotify API changes. No audio features meant the core dynamic element wasn't feasible. Time to pivot. With the hardware already in place, the focus shifted to making the 8x32 LED matrix display the available information—primarily the track name—as effectively and aesthetically as possible.

The new goal: get the track name scrolling smoothly across the matrix, using colors derived from the album art palette (data that *is* still accessible). This presented its own set of technical hurdles.

## Implementation: Libraries, Matrix Mapping, and Fonts

First was selecting the right LED library. FastLED, while powerful for animations, presented challenges for straightforward text rendering without significant custom font work. Switching back to the Adafruit_NeoMatrix and GFX libraries offered built-in text functions, but required careful configuration to match the specific wiring layout of my 8x32 panel (which turned out to be a column-major, zigzag pattern). This involved a process of elimination: running test patterns (sequential fills, corner pixel checks) and comparing the output to the library's coordinate system until the correct constructor flags (`NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG`) were identified. This step ensured `drawPixel(x,y)` behaved predictably.

With the matrix mapping solved, the next step was scrolling text. The default 5x7 Adafruit font worked, but I briefly explored creating a custom 3x5 font to fit the text within the top half of the display. Implementing this proved tricky, with alignment and rendering issues being difficult to resolve cleanly. For simplicity and reliability, I reverted to the standard full-height font.

## Implementation: Animation Smoothing and Color Gradients

Achieving smooth scrolling introduced another challenge. Network requests on the Arduino are blocking operations. When the device fetches data from the API, the animation pauses, causing a noticeable stutter. Reducing the polling interval didn't eliminate this. I experimented with synchronizing API calls to the animation cycle (e.g., fetching only when the text wraps around), using flags to manage state. While partially effective, this added complexity and potential edge cases, particularly around WiFi reconnection logic. Ultimately, I returned to a simpler time-based polling mechanism (currently every 30 seconds) and accepted the minor stutter as a limitation of the platform's handling of blocking network calls.

Finally, enhancing the visual appeal involved using the color palette data from the API. Initially, the text was colored with just the dominant color. This evolved into applying a gradient across the text, interpolating between the available palette colors on a per-character basis. While a per-pixel gradient would be ideal, the standard GFX text rendering functions apply color per character. The current implementation calculates the target color for each pixel column but applies it to the character block containing that column, resulting in a pleasant shimmer effect as the text scrolls.

The result of this pivot is a scrolling text display showing the current track name, colored with a dynamic gradient based on the album art. It experiences a brief pause during data fetches due to the nature of blocking network calls on the Arduino. The journey involved navigating library choices, hardware mapping, font rendering limitations, and the constraints of the microcontroller environment. While different from the original audio-reactive concept, it successfully visualizes the current music playback using the available data and hardware.

## Final Thoughts

If you’re reading this and thinking about building something similar: check the API changelog first. If you want to do anything interesting with Spotify data, you’re out of luck. The most compelling part of the API is simply gone. For now, this project is just a progress bar with album art.

If Spotify ever brings back audio features, maybe I’ll revisit this. But for now, that’s the end of the road.
