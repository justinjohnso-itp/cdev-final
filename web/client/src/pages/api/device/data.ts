import type { APIRoute } from 'astro';
import { supabase } from '../../../supabase';
import SpotifyWebApi from 'spotify-web-api-node';

const CLIENT_ID = import.meta.env.SPOTIFY_CLIENT_ID;
const CLIENT_SECRET = import.meta.env.SPOTIFY_CLIENT_SECRET;
const REDIRECT_URI = import.meta.env.SPOTIFY_REDIRECT_URI;

export const prerender = false;

async function getLatestSpotifyTokens() {
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
  if (new Date(expires_at) < new Date()) {
    try {
      const refreshed = await refreshAccessToken(refresh_token);
      access_token = refreshed.access_token;
      expires_at = new Date(Date.now() + refreshed.expires_in * 1000).toISOString();
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
    if (!playback.body || !playback.body.item) {
      return new Response(JSON.stringify({ isPlaying: false }), { status: 200 });
    }
    const track = playback.body.item;
    const progress_ms = playback.body.progress_ms;
    const duration_ms = track.duration_ms;
    const timestamp = playback.body.timestamp;
    const isPlaying = playback.body.is_playing;
    const albumArt = track.album.images && track.album.images.length > 0 ? track.album.images[0].url : null;
    return new Response(JSON.stringify({
      isPlaying,
      progress_ms,
      duration_ms,
      timestamp,
      track: {
        id: track.id,
        name: track.name,
        artists: track.artists.map((a: any) => a.name),
        album: track.album.name,
        albumArt,
      },
      device: playback.body.device ? {
        name: playback.body.device.name,
        type: playback.body.device.type,
        volume_percent: playback.body.device.volume_percent,
      } : null,
      shuffle_state: playback.body.shuffle_state,
      repeat_state: playback.body.repeat_state,
    }), { status: 200 });
  } catch (err) {
    return new Response(JSON.stringify({ error: 'Failed to fetch playback info' }), { status: 500 });
  }
};
