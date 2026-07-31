import { Hono } from 'hono'
import { serve } from '@hono/node-server'
import { serveStatic } from '@hono/node-server/serve-static'
import { readFile } from 'node:fs/promises'
import { resolve, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const __filename = fileURLToPath(import.meta.url)
const __dirname = dirname(__filename)

/**
 * Static files root directory.
 * Defaults to `frontend/dist` relative to the project root.
 * Override with STATIC_ROOT env var.
 */
const STATIC_ROOT = resolve(
  process.env.STATIC_ROOT || resolve(__dirname, '../../frontend/dist')
)

const app = new Hono()

// ── API routes (registered before static middleware) ──────────────
app.get('/api/health', (c) => c.json({ status: 'ok' }))

// ── Static file serving ───────────────────────────────────────────
// Serves existing files from STATIC_ROOT. Falls through via next()
// when the file does not exist (SPA-friendly).
app.use('/*', serveStatic({ root: STATIC_ROOT }))

// ── SPA fallback ──────────────────────────────────────────────────
// For any unmatched GET request, serve index.html.
app.get('*', async (c) => {
  try {
    const indexHtml = await readFile(resolve(STATIC_ROOT, 'index.html'), 'utf-8')
    return c.html(indexHtml)
  } catch {
    return c.notFound()
  }
})

// ── Start server (only when run directly) ─────────────────────────
const PORT = parseInt(process.env.PORT || '3000', 10)

if (process.argv[1] && import.meta.url.endsWith(process.argv[1])) {
  serve({ fetch: app.fetch, port: PORT })
  console.log(`Server listening on http://localhost:${PORT}`)
}

export default app
