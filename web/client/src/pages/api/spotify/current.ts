
import type { APIRoute } from 'astro';
import { createClient } from '@supabase/supabase-js';
import SpotifyWebApi from 'spotify-web-api-node';

const supabaseUrl = import.meta.env.SUPABASE_URL;
const supabaseAnonKey = import.meta.env.SUPABASE_ANON_KEY;

if (!supabaseUrl || !supabaseAnonKey) {
  throw new Error("Supabase URL or Anon Key not configured.");
}
const supabase = createClient(supabaseUrl, supabaseAnonKey);

async function getLatestSpotifyTokens() {
  const { data, error } = await supabase
    .from('spotify_tokens')
    .select('*')
    .order('last_updated_at', { ascending: false })
    .limit(1)
    .single();
  if (error) {
    console.error('Error fetching Spotify tokens from Supabase:', error.message);
    return null;
  }
  if (!data) {
    console.error('No Spotify tokens found in Supabase.');
    return null;
  }
  return data;
}

async function refreshAccessToken(refreshToken: string, spotifyUserId: string) {
  const spotifyApi = new SpotifyWebApi({
    clientId: import.meta.env.SPOTIFY_CLIENT_ID,
    clientSecret: import.meta.env.SPOTIFY_CLIENT_SECRET,
    redirectUri: import.meta.env.SPOTIFY_REDIRECT_URI,
    refreshToken: refreshToken,
  });

  try {
    const data = await spotifyApi.refreshAccessToken();
    console.log('Spotify token refreshed successfully via API route.');
    const newAccessToken = data.body['access_token'];
    const newExpiresAt = new Date(Date.now() + data.body['expires_in'] * 1000).toISOString();

    // Update Supabase with the new token and expiry
    const { error: updateError } = await supabase
      .from('spotify_tokens')
      .update({
        access_token: newAccessToken,
        expires_at: newExpiresAt,
        last_updated_at: new Date().toISOString(),
      })
      .eq('spotify_user_id', spotifyUserId);

    if (updateError) {
      console.error('Error updating refreshed token in Supabase (API route):', updateError.message);
    }
    return newAccessToken;
  } catch (err: any) {
    console.error('Error refreshing Spotify token (API route):', err.message || err);
    throw err;
  }
}

export const GET: APIRoute = async ({ request }) => {
  try {
    let tokens = await getLatestSpotifyTokens();

    if (!tokens) {
      return new Response(JSON.stringify({ error: 'Could not retrieve Spotify tokens.' }), { status: 500 });
    }

    let { access_token, refresh_token, expires_at, spotify_user_id } = tokens;

    if (new Date(expires_at) < new Date()) {
      console.log('Token expired, refreshing via API route...');
      try {
        access_token = await refreshAccessToken(refresh_token, spotify_user_id);
      } catch (refreshErr) {
        return new Response(JSON.stringify({ error: 'Failed to refresh Spotify token.' }), { status: 500 });
      }
    }

    const spotifyApi = new SpotifyWebApi({ accessToken: access_token });
    const playbackState = await spotifyApi.getMyCurrentPlaybackState();

    if (!playbackState.body || !playbackState.body.item) {
      return new Response(JSON.stringify({ isPlaying: false, message: 'Nothing currently playing or device not active.' }), { status: 200 });
    }

    const track = playbackState.body.item;
    const albumArtUrl = track.album.images && track.album.images.length > 0 ? track.album.images[0].url : null;
    
    // Note: ColorThief is not used here to keep the API route light.
    // The server-side component handles palette generation for the MQTT messages.

    const currentSongData = {
      isPlaying: playbackState.body.is_playing,
      progress_ms: playbackState.body.progress_ms,
      duration_ms: track.duration_ms,
      timestamp: playbackState.body.timestamp,
      track: {
        id: track.id,
        name: track.name,
        artists: track.artists.map((a: any) => a.name),
        album: track.album.name,
        albumArtUrl: albumArtUrl,
      },
      device: playbackState.body.device ? {
        name: playbackState.body.device.name,
        type: playbackState.body.device.type,
        volume_percent: playbackState.body.device.volume_percent,
      } : null,
    };

    return new Response(JSON.stringify(currentSongData), {
      status: 200,
      headers: { 'Content-Type': 'application/json' },
    });

  } catch (error: any) {
    console.error('Error in /api/spotify/current:', error.message || error);
    return new Response(JSON.stringify({ error: 'Internal Server Error', details: error.message }), { status: 500 });
  }
};
