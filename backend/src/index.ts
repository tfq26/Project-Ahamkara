import { serve } from '@hono/node-server';
import { serveStatic } from '@hono/node-server/serve-static';
import { Hono } from 'hono';
import { cors } from 'hono/cors';
import { api } from './routes/index.js';

const app = new Hono();

// CORS for development
app.use('/*', cors());

// API routes
app.route('/api', api);

// Static file serving for the built frontend
const publicDir = process.env.PUBLIC_DIR || '../frontend/dist';
app.use('/*', serveStatic({ root: publicDir }));

// Fallback: serve index.html for SPA routing
app.get('*', serveStatic({ path: 'index.html', root: publicDir }));

const port = Number(process.env.PORT) || 3000;
const hostname = process.env.HOST || '0.0.0.0';

serve({ fetch: app.fetch, port, hostname }, (info) => {
  console.log(`Ahamkara backend listening on http://${info.address}:${info.port}`);
});

export default app;
