# Ahamkara Backend

Hono-based platform backend server. Serves the built frontend and provides API endpoints.

## Prerequisites

- Node.js >= 20
- npm >= 9

## Setup

```sh
cd backend
npm install
```

## Development

```sh
npm run dev
```

Starts the server with hot reload on `http://0.0.0.0:3000`.

## Build & Start

```sh
npm run build
npm start
```

## Configuration

| Variable     | Default           | Description                        |
|--------------|-------------------|------------------------------------|
| `PORT`       | `3000`            | Server port                        |
| `HOST`       | `0.0.0.0`         | Server bind address                |
| `PUBLIC_DIR` | `../frontend/dist` | Path to the built frontend output  |

## API Endpoints

| Method | Path           | Description    |
|--------|----------------|----------------|
| GET    | `/api/health`  | Health check   |

## Testing

```sh
npm test
```
