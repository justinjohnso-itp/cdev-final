---
title: "Spotify LED Visualizer Dev Log 3: MQTT, New Features, and a UI!"
date: 2025-05-06
tags: ["spotify", "led visualizer", "mqtt", "arduino", "nodejs", "astro", "iot", "devlog"]
---

Alright, it's May 6th, and it's been a pretty packed day working on the Spotify LED Visualizer. After hitting that wall with the Spotify API deprecating audio features (see Dev Log 2), I've been focusing on making the most of what data I *can* get. Today was all about a major refactor and adding some much-needed enhancements to both the Arduino display and the web client.

## Moving to MQTT: No More Polling!

The biggest change today was ditching the old HTTP polling system. If you remember from Dev Log 1, the Arduino was constantly bugging the server for updates. It worked, but it felt clunky and wasn't great for instant updates when a song changed.

So, I decided to switch things over to **MQTT**. Here's the new setup:
*   **Arduino Side (`main.cpp`):** I rewrote a good chunk of the Arduino code to use the `ArduinoMqttClient` library. Now, instead of asking, it just listens on an MQTT topic (`conndev/spotify-visualizer`) for new song data.
*   **Server Side (`web/server/index.js`):** The Node.js server is now an MQTT publisher. It still does the job of grabbing the current song from Spotify (using Supabase for the token management, which is way better than my old file-based thing), but then it blasts that info out over MQTT.

The beauty of this is that the Arduino gets updates pretty much the instant they happen. Change a song, hit pause – the LEDs should react way faster now. This should make the whole thing feel a lot more responsive.

## Arduino Enhancements: Finer Control and Smoother Visuals

With MQTT handling the data flow, I could finally spend some time sprucing up what's actually showing on the LED matrix.

### 1. A New Font for Tighter Spaces
That old default font was okay, but track names can get pretty long, and on a 32x8 matrix, every pixel counts. I found a more **compact custom font (`5x5_6pt7b.h`)** that looked like it would fit better. Getting it working involved:
*   Dropping the new font file into the Arduino project's include directory.
*   Tweaking `main.cpp` to use `matrix.setFont()` and `matrix.getTextBounds()`. The `getTextBounds` part is super important for figuring out exactly how wide the text is, which is key for making the scroll look smooth.
*   I also had to rename a few things inside the font file itself. Some of the C++ names it came with were a bit wonky and didn't compile.

### 2. Something to Look at When Nothing's Playing: The Sine Wave
Previously, if Spotify wasn't playing, the display was just... off. Not very exciting, and it didn't tell you if the thing was even working. So, I added an **idle animation: a scrolling sine wave**.

*How it works (or how I think it works):*
*   For every column on the LED matrix, I calculate a Y position using `sin()`.
*   There's a `waveOffset` that I change a little bit each frame, which makes the wave look like it's moving across the screen.
*   To make it a bit fancier, the color of the wave slowly cycles through a rainbow using `matrix.ColorHSV()`.
*   A little `delay()` keeps the animation speed in check.

Now, even if there's no music, there's some life in the display, and it's a good sign that it's alive and waiting for data.

### 3. You Control the Scroll Speed!
I thought it'd be cool to let whoever's looking at it have some control. So, I hooked up a potentiometer to analog pin `A0`. Now, you can **tweak the scroll speed of the track info** in real-time.

*The nitty-gritty:*
*   The Arduino reads the potentiometer value (0-1023).
*   I use the `map()` function to change that raw value into a delay amount (something like 20ms to 200ms). Less delay means faster scrolling.
*   This `currentScrollDelay` then gets used in the text scrolling loop.

It's a small touch, but I like giving people options.

### 4. That Annoying Flash (Still Chasing This One)
There's still one gremlin in the Arduino code: the display **flashes for a split second when it switches from scrolling a song title to the idle animation (and vice-versa)**. It's driving me a bit nuts.
*   My latest attempt to fix it was to make sure I'm calling `matrix.fillScreen(0)` (to clear everything) right before the idle animation starts.
*   I'd already tried removing some `clearMatrix()` and `matrix.show()` calls that I thought might be redundant. Still no luck. This one's still on the to-do list.

## Making it Look Less Like a Science Project: The Enclosure
Software is great, but I also wanted this thing to look decent, not just like a pile of wires. So, I spent some time designing and putting together a custom enclosure.

*   **The Idea:** I wanted something clean and simple that wouldn't look out of place on a desk.
*   **The Bits and Pieces:**
    *   **Acrylic Sandwich:** The LED matrix panel is sandwiched between two pieces of acrylic. This protects the LEDs and also diffuses the light a bit, which makes the text easier to read and a bit softer on the eyes.
    *   **Laser-Cut Plywood Stand:** The main body is a stand I laser-cut from plywood. It holds the acrylic panel and has space inside to hide the Arduino and wires.

It definitely looks more like a "thing" now, rather than just a bunch of electronics.

## Giving the Web Client Some Love: The "Now Playing" Page
The web client (the Astro site) was pretty barebones. It just had the login link. I decided it needed a proper **"Now Playing" section** so you could actually see what Spotify was, well, playing, without having to open the Spotify app.

Here’s what went into that:
1.  **New API Route:** I built a new API endpoint at `/web/client/src/pages/api/spotify/current.ts`. This guy is responsible for fetching all the current playback info from Spotify and dealing with refreshing the access token if it's expired.
2.  **Astro Page Update**: I updated the main `index.astro` page to:
    *   Call this new API when the page loads (it does this on the server before sending the page to the browser).
    *   Display all the good stuff:
        *   Album Art
        *   Track Name
        *   Artist(s)
        *   Album Name
        *   A progress bar showing how far into the song you are.
        *   What device is playing (e.g., "My Echo Dot") and its volume.
    *   I also styled it up a bit with a dark theme and some Spotify green. Looks pretty slick, if I do say so myself.
    *   **Update (Post-Initial Write-up):** I then refactored this page to fetch data client-side every second using `setInterval` and JavaScript to update the DOM directly. This makes the progress bar and track info update in real-time without full page reloads, which is much cooler. I also had to fix an issue where the fetch URL was hardcoded to `localhost` for deployed builds; it now correctly uses `Astro.site` or the production URL.

This makes the web client way more useful.

## Tidying Up
I also did a little spring cleaning and got rid of the old `/web/client/src/pages/api/device/data.ts` API route. That was from the pre-MQTT days and isn't needed anymore. Also zapped the old Netlify function config for it. Keep it clean!

## What's Next?
So, a pretty good burst of progress. The main things still on my plate are:
*   **Nail down that flashing issue** on the Arduino. It's subtle, but it bugs me.
*   **Serious end-to-end testing.** Now that so much has changed, I need to really hammer on it:
    *   Make sure MQTT is solid.
    *   Check the potentiometer scroll speed is smooth and responsive.
    *   Just generally try to break the server and client to see what shakes loose.

More updates to come as I (hopefully) squash the remaining bugs and polish it up!
