# Ahamkara Backend

<<<<<<< HEAD
Hono-based HTTP backend that serves the Ahamkara frontend SPA with static file support and client-side routing fallback.
=======
Hono web server that serves the Vue.js info website.
>>>>>>> forgejo/issue-4b85475d

## Quick start

```bash
<<<<<<< HEAD
cd backend
npm install
npm run dev    # development with hot-reload
npm start      # production
```

## Configuration

| Variable      | Default                        | Description                              |
|---------------|--------------------------------|------------------------------------------|
| `PORT`        | `3000`                         | HTTP server port                         |
| `STATIC_ROOT` | `<repo>/frontend/dist` | Path to built frontend assets            |

## Architecture

```
Request → API routes → Static file middleware → SPA fallback (index.html)
```

1. **API routes** (e.g. `/api/health`) are registered first and short-circuit.
2. **Static middleware** serves existing files from `STATIC_ROOT`; calls `next()` when a file is not found.
3. **SPA fallback** returns `index.html` for any unmatched GET request, enabling client-side routing.

## Testing

```bash
npm test
```

Uses [vitest](https://vitest.dev/) with Hono's built-in `app.request()` for integration tests without a running server.
=======
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
>>>>>>> forgejo/issue-4b85475d
