import { describe, it, expect } from 'vitest'
import app from './server'

describe('Hono backend', () => {
  it('serves / as HTML', async () => {
    const res = await app.request('/')
    expect(res.status).toBe(200)
    expect(res.headers.get('content-type')).toContain('text/html')
  })

  it('SPA fallback for /docs', async () => {
    const res = await app.request('/docs')
    expect(res.status).toBe(200)
    expect(res.headers.get('content-type')).toContain('text/html')
  })

  it('SPA fallback for unknown routes', async () => {
    const res = await app.request('/some/deep/route')
    expect(res.status).toBe(200)
    expect(res.headers.get('content-type')).toContain('text/html')
  })
})
