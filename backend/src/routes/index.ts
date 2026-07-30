import { Hono } from 'hono';
import { health } from './health.js';

export const api = new Hono();

api.route('/health', health);

// Future API routes can be added here:
// import { auth } from './auth.js';
// api.route('/auth', auth);
