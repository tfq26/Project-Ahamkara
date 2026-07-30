# Ahamkara Backend

Hono web server that serves the Vue.js info website.

## Quick start

```bash
# install deps for backend + frontend
npm install
npm --prefix frontend install

# build the Vue frontend (outputs to frontend/dist/)
npm run build:frontend

# start the Hono server (default port 3000)
npm run dev
```

The server is available at `http://localhost:3000`.

## Integration

The Hono backend (`src/server.ts`) serves static assets from `frontend/dist/`
using Hono's `serveStatic` middleware. All non-file routes fall back to
`index.html` so the Vue SPA handles client-side routing — direct navigation
to `/docs` and any future frontend route works without 404s.

To change the port, set the `PORT` environment variable.

## Build pipeline

```
backend/frontend/   →  npm run build  →  frontend/dist/
backend/src/        →  npm run build  →  dist/          (optional TS compile)
backend/            npm run dev        serves frontend/dist/ at :3000
```

## API

No API routes defined yet. Add routes before the catch-all `notFound`
handler in `src/server.ts`.
