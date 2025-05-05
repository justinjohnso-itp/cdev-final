// Scheduled function to periodically invoke the Spotify data API endpoint
import fetch from 'node-fetch';

export const handler = async (event, context) => {
  // Build the full URL to your API route
  const baseUrl = process.env.URL || process.env.DEPLOY_PRIME_URL;
  if (!baseUrl) {
    console.error('BASE URL not set');
    return {
      statusCode: 500,
      body: JSON.stringify({ status: 'error', message: 'BASE URL not configured' })
    };
  }

  const endpoint = `${baseUrl.replace(/\/$/, '')}/api/device/data`;
  console.log(`Triggering scheduled fetch of Spotify data: ${endpoint}`);

  try {
    const response = await fetch(endpoint);
    const text = await response.text();
    console.log(`Fetch response: ${response.status} - ${text}`);
    return {
      statusCode: 200,
      body: JSON.stringify({ status: 'success', data: text })
    };
  } catch (err) {
    console.error('Scheduled fetch error:', err);
    return {
      statusCode: 500,
      body: JSON.stringify({ status: 'error', message: err.message })
    };
  }
};
