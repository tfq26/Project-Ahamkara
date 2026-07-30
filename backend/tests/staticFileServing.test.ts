import { describe, it, expect } from 'vitest'
import app from '../src/server'

// The default STATIC_ROOT points to backend/../../frontend/dist = frontend/dist
// which is correct when tests run from the backend/ directory.

describe('Static file serving', () => {
  it('serves existing HTML files', async () => {
    const res = await app.request('/')
    expect(res.status).toBe(200)
    expect(res.headers.get('content-type')).toMatch(/text\/html/)
    const text = await res.text()
    expect(text).toContain('Ahamkara')
  })

  it('serves existing CSS files with correct content type', async () => {
    const res = await app.request('/style.css')
    expect(res.status).toBe(200)
    expect(res.headers.get('content-type')).toMatch(/text\/css/)
  })

  it('serves existing JS files', async () => {
    const res = await app.request('/app.js')
    expect(res.status).toBe(200)
    // Hono maps .js to text/javascript per the MIME spec
    expect(res.headers.get('content-type')).toMatch(/text\/javascript/)
  })

  it('serves files in subdirectories', async () => {
    const res = await app.request('/assets/logo.svg')
    expect(res.status).toBe(200)
    expect(res.headers.get('content-type')).toMatch(/image\/svg/)
  })
})

describe('SPA fallback', () => {
  it('returns index.html for unknown routes (deep links)', async () => {
    const res = await app.request('/about')
    expect(res.status).toBe(200)
    expect(res.headers.get('content-type')).toMatch(/text\/html/)
    const text = await res.text()
    expect(text).toContain('Ahamkara')
  })

  it('returns index.html for nested deep links', async () => {
    const res = await app.request('/dashboard/settings')
    expect(res.status).toBe(200)
    const text = await res.text()
    expect(text).toContain('Ahamkara')
  })

  it('returns index.html for root path', async () => {
    const res = await app.request('/')
    expect(res.status).toBe(200)
    const text = await res.text()
    expect(text).toContain('Loading...')
  })
})

describe('API routes', () => {
  it('serves /api/health without fallback interference', async () => {
    const res = await app.request('/api/health')
    expect(res.status).toBe(200)
    expect(res.headers.get('content-type')).toMatch(/application\/json/)
    const body = await res.json()
    expect(body).toEqual({ status: 'ok' })
  })
})
