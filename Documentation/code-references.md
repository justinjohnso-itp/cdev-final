# Code References

This file tracks external code sources, tutorials, documentation pages, and concepts used or adapted during the development of the Spotify LED Visualizer project.

## Arduino (`/arduino`)

- **Adafruit NeoPixel Library Example (`strandtest.ino`)**: Used as a basis for initializing the WS2812B LED strip and implementing the initial `colorWipe` test pattern in `main.cpp`.
  - *Source:* [Adafruit NeoPixel Library GitHub](https://github.com/adafruit/Adafruit_NeoPixel)
- **Adafruit NeoMatrix & Adafruit GFX**: Used for matrix text rendering and scrolling text effects on the LED matrix.
  - *Source:* [Adafruit NeoMatrix](https://github.com/adafruit/Adafruit_NeoMatrix), [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library)
- **ArduinoJson**: Used for parsing JSON responses from the web API, including robust handling of arrays (palette, dominantColor) with `JsonArrayConst`.
  - *Source:* [ArduinoJson](https://arduinojson.org/)
- **ArduinoHttpClient**: Used for making HTTPS requests to the Netlify-hosted Astro API endpoint.
  - *Source:* [ArduinoHttpClient GitHub](https://github.com/arduino-libraries/ArduinoHttpClient)
- **WiFiNINA**: Used for WiFi connectivity on Arduino Nano 33 IoT.
  - *Source:* [WiFiNINA GitHub](https://github.com/arduino-libraries/WiFiNINA)

## Web (`/web`)

- **Astro**: Used as the web framework for the backend/API, deployed to Netlify.
  - *Source:* [Astro Docs](https://docs.astro.build/)
- **Supabase**: Used for user authentication and token storage.
  - *Source:* [Supabase Docs](https://supabase.com/docs)
- **Spotify Web API Node library**: Used for interacting with the Spotify Web API, including handling OAuth 2.0 authentication and making API calls.
  - *Source:* [spotify-web-api-node GitHub](https://github.com/thelinmichael/spotify-web-api-node)
- **Spotify Authorization Code Flow**: The OAuth 2.0 flow implemented for user authentication.
  - *Source:* [Spotify Authorization Guide](https://developer.spotify.com/documentation/web-api/tutorials/code-flow)
- **Spotify API Scopes**: Used to define the permissions requested from the user during authorization.
  - *Source:* [Spotify Scopes Documentation](https://developer.spotify.com/documentation/web-api/concepts/scopes)
- **colorthief**: Used for server-side color extraction from album art images.
  - *Source:* [ColorThief npm](https://www.npmjs.com/package/colorthief)
- **node-fetch**: Used for server-side HTTP requests to fetch album art images for color extraction.
  - *Source:* [node-fetch npm](https://www.npmjs.com/package/node-fetch)
- **@astrojs/netlify**: Used to deploy the Astro app as serverless functions on Netlify.
  - *Source:* [Astro Netlify Adapter](https://docs.astro.build/en/guides/deploy/netlify/)

## General Concepts & Documentation

- **Spotify API Deprecation Notice**: Noted that `/audio-features` endpoint is deprecated (returns 403), and project pivoted to use only playback/track metadata and album art colors.
  - *Source:* [Spotify API Docs](https://developer.spotify.com/documentation/web-api/reference/get-audio-features)
- **ArduinoJson Array Parsing**: Used ArduinoJson documentation and GitHub issues to resolve parsing of arrays as `JsonArrayConst` for robust error-free code.
  - *Source:* [ArduinoJson Array Docs](https://arduinojson.org/v6/api/jsonarray/)

## Project Management

- **Netlify Deployment**: Used Netlify for serverless deployment, including configuration with `netlify.toml` and environment variables.
  - *Source:* [Netlify Docs](https://docs.netlify.com/)

## Attribution

All referenced code and libraries are used under their respective open source licenses. See individual repositories for details.
