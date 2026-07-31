# Ahamkara Backend

Hono-based HTTP backend for the Ahamkara project. Serves the built Vue.js frontend and provides API endpoints.

## Quick Start

```bash
# Install dependencies
npm install

# Start development server (with hot reload)
npm run dev

# Build TypeScript
npm run build

# Start production server
npm start
```

## API Endpoints

| Method | Path          | Description        |
| ------ | ------------- | ------------------ |
| GET    | `/api/health` | Health check       |

### Health Check

```bash
curl http://localhost:3000/api/health
# → { "status": "ok", "timestamp": "2026-07-30T21:00:00.000Z" }
```

## Frontend

The static file server serves the built frontend from `./dist/`. To serve your Vue.js app:

```bash
# Build the frontend and copy output to backend/dist/
npm run build  # in the frontend project
cp -r frontend/dist/* backend/dist/
```

## Configuration

Configuration is via environment variables (see `.env.example`):

| Variable | Default | Description          |
| -------- | ------- | -------------------- |
| `PORT`   | `3000`  | HTTP server port     |

## Tests

```bash
npm test
```
