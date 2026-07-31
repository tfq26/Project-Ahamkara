# Ahamkara Backend

Hono-based HTTP backend that serves the Ahamkara frontend SPA with static file support and client-side routing fallback.

## Quick start

```bash
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
