import type { APIRoute } from 'astro';
import { supabase } from '../../../supabase';
import SpotifyWebApi from 'spotify-web-api-node';

const CLIENT_ID = import.meta.env.SPOTIFY_CLIENT_ID;
const CLIENT_SECRET = import.meta.env.SPOTIFY_CLIENT_SECRET;
const REDIRECT_URI = import.meta.env.SPOTIFY_REDIRECT_URI;

export const prerender = false;

export const GET: APIRoute = async (context) => {
  // Try to extract code from context.request.url (raw request URL)
  const rawUrl = context.request.url;
  const fullUrl = rawUrl.startsWith('http') ? rawUrl : `http://127.0.0.1:4321${rawUrl}`;
  const urlObj = new URL(fullUrl);
  const code = urlObj.searchParams.get('code');
  if (!code) {
    return new Response('Missing code', { status: 400 });
  }

  const spotifyApi = new SpotifyWebApi({
    clientId: CLIENT_ID,
    clientSecret: CLIENT_SECRET,
    redirectUri: REDIRECT_URI,
  });
  try {
    const data = await spotifyApi.authorizationCodeGrant(code);
    const { access_token, refresh_token, expires_in } = data.body;
    spotifyApi.setAccessToken(access_token);
    spotifyApi.setRefreshToken(refresh_token);
    const me = await spotifyApi.getMe();
    const spotify_user_id = me.body.id;
    const display_name = me.body.display_name;
    const expires_at = new Date(Date.now() + expires_in * 1000).toISOString();
    await supabase.from('spotify_tokens').upsert({
      spotify_user_id,
      access_token,
      refresh_token,
      expires_at,
      last_updated_at: new Date().toISOString(),
      display_name,
    }, { onConflict: 'spotify_user_id' });
    return context.redirect('/');
  } catch (err) {
    return new Response('Spotify auth failed', { status: 500 });
  }
};
