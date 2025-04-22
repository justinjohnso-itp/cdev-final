# Code References

This file tracks external code sources, tutorials, documentation pages, and concepts used or adapted during the development of the Spotify LED Visualizer project.

## Arduino (`/arduino`)

- **Adafruit NeoPixel Library Example (`strandtest.ino`)**: Used as a basis for initializing the WS2812B LED strip and implementing the initial `colorWipe` test pattern in `main.cpp`.
  - *Source:* [Adafruit NeoPixel Library GitHub](https://github.com/adafruit/Adafruit_NeoPixel)

## Web (`/web`)

- **Spotify Web API Node library**: Used for interacting with the Spotify Web API, including handling OAuth 2.0 authentication and making API calls.
  - *Source:* [spotify-web-api-node GitHub](https://github.com/thelinmichael/spotify-web-api-node)
- **Spotify Authorization Code Flow**: The OAuth 2.0 flow implemented for user authentication.
  - *Source:* [Spotify Authorization Guide](https://developer.spotify.com/documentation/web-api/tutorials/code-flow)
- **Spotify API Scopes**: Used to define the permissions requested from the user during authorization.
  - *Source:* [Spotify Scopes Documentation](https://developer.spotify.com/documentation/web-api/concepts/scopes)
