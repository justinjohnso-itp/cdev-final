import type { APIRoute } from 'astro';
import { supabase } from '../../../supabase';
import SpotifyWebApi from 'spotify-web-api-node';

const CLIENT_ID = import.meta.env.SPOTIFY_CLIENT_ID;
const CLIENT_SECRET = import.meta.env.SPOTIFY_CLIENT_SECRET;
const REDIRECT_URI = import.meta.env.SPOTIFY_REDIRECT_URI;

async function getLatestSpotifyTokens() {
  // Get the most recently updated token
  const { data, error } = await supabase
    .from('spotify_tokens')
    .select('*')
    .order('last_updated_at', { ascending: false })
    .limit(1)
    .single();
  if (error || !data) return null;
  return data;
}

async function refreshAccessToken(refresh_token: string) {
  const spotifyApi = new SpotifyWebApi({
    clientId: CLIENT_ID,
    clientSecret: CLIENT_SECRET,
    redirectUri: REDIRECT_URI,
    refreshToken: refresh_token,
  });
  const data = await spotifyApi.refreshAccessToken();
  return data.body;
}

export const GET: APIRoute = async () => {
  const tokens = await getLatestSpotifyTokens();
  if (!tokens) {
    return new Response(JSON.stringify({ error: 'No Spotify tokens found' }), { status: 404 });
  }
  let { access_token, refresh_token, expires_at, spotify_user_id } = tokens;
  // Check if token is expired
  if (new Date(expires_at) < new Date()) {
    try {
      const refreshed = await refreshAccessToken(refresh_token);
      access_token = refreshed.access_token;
      expires_at = new Date(Date.now() + refreshed.expires_in * 1000).toISOString();
      // Update tokens in Supabase
      await supabase.from('spotify_tokens').update({
        access_token,
        expires_at,
        last_updated_at: new Date().toISOString(),
      }).eq('spotify_user_id', spotify_user_id);
    } catch (err) {
      return new Response(JSON.stringify({ error: 'Failed to refresh token' }), { status: 500 });
    }
  }
  const spotifyApi = new SpotifyWebApi({
    clientId: CLIENT_ID,
    clientSecret: CLIENT_SECRET,
    redirectUri: REDIRECT_URI,
    accessToken: access_token,
  });
  try {
    const playback = await spotifyApi.getMyCurrentPlaybackState();
    console.log('Spotify playback response:', JSON.stringify(playback.body, null, 2));
    if (!playback.body || !playback.body.is_playing || !playback.body.item) {
      return new Response(JSON.stringify({ isPlaying: false }), { status: 200 });
    }
    const track = playback.body.item;
    const progress_ms = playback.body.progress_ms;
    const timestamp = playback.body.timestamp;
    try {
      const features = await spotifyApi.getAudioFeaturesForTrack(track.id);
      return new Response(JSON.stringify({
        isPlaying: true,
        progress_ms,
        timestamp,
        track: {
          id: track.id,
          name: track.name,
          artists: track.artists.map((a: any) => a.name),
          album: track.album.name,
        },
        features: features.body,
      }), { status: 200 });
    } catch (featureErr: any) {
      // If 403, return playback info with features: null
      if (featureErr?.statusCode === 403) {
        console.warn('Audio features forbidden for track:', track.id, track.name);
        if (featureErr.body) {
          console.warn('Spotify error body:', JSON.stringify(featureErr.body));
        }
        return new Response(JSON.stringify({
          isPlaying: true,
          progress_ms,
          timestamp,
          track: {
            id: track.id,
            name: track.name,
            artists: track.artists.map((a: any) => a.name),
            album: track.album.name,
          },
          features: null,
          error: 'Audio features not available for this track.'
        }), { status: 200 });
      }
      // Log all other errors
      console.error('Error fetching audio features:', featureErr);
      if (featureErr.body) {
        console.error('Spotify error body:', JSON.stringify(featureErr.body));
      }
      return new Response(JSON.stringify({ error: 'Failed to fetch audio features' }), { status: 500 });
    }
  } catch (err) {
    console.error('Error fetching playback:', err);
    return new Response(JSON.stringify({ error: 'Failed to fetch playback or features' }), { status: 500 });
  }
};
