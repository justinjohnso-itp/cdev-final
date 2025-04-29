import type { APIRoute } from 'astro';

const CLIENT_ID = import.meta.env.SPOTIFY_CLIENT_ID;
const REDIRECT_URI = import.meta.env.SPOTIFY_REDIRECT_URI; // Now uses 127.0.0.1
const SCOPES = [
  'user-read-currently-playing',
  'user-read-playback-state',
  'user-read-email',
  'user-read-private',
].join(' ');

export const GET: APIRoute = async ({ redirect }) => {
  const params = new URLSearchParams({
    response_type: 'code',
    client_id: CLIENT_ID,
    scope: SCOPES,
    redirect_uri: REDIRECT_URI,
  });
  return redirect(`https://accounts.spotify.com/authorize?${params.toString()}`);
};
