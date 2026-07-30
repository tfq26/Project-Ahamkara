import { describe, expect, it } from 'vitest';
import { health } from '../routes/health.js';

describe('GET /api/health', () => {
  it('returns 200 OK with status and timestamp', async () => {
    const res = await health.request('/');
    expect(res.status).toBe(200);

    const body = await res.json();
    expect(body).toHaveProperty('status', 'ok');
    expect(body).toHaveProperty('timestamp');
    expect(body).toHaveProperty('uptime');
    expect(typeof body.uptime).toBe('number');
  });
});
