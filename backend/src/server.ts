import { Hono } from 'hono'
import { serveStatic } from '@hono/node-server/serve-static'
import { serve } from '@hono/node-server'
import { readFileSync } from 'node:fs'
import { join } from 'node:path'

const dist = join(import.meta.dirname, '../frontend/dist')

const app = new Hono()

app.use('/assets/*', serveStatic({ root: dist }))
app.use('/favicon.ico', serveStatic({ root: dist }))
app.use('/*', serveStatic({ root: dist, rewriteRequestPath: (p) => p === '/' ? '/index.html' : p }))

app.notFound((c) => {
  try {
    return c.html(readFileSync(join(dist, 'index.html'), 'utf-8'))
  } catch {
    return c.text('Not Found', 404)
  }
})

export default app

const isDirectRun = process.argv[1]?.includes('/src/server')
if (isDirectRun) {
  const port = Number(process.env.PORT) || 3000
  console.log(`Backend listening on http://localhost:${port}`)
  serve({ fetch: app.fetch, port })
}
