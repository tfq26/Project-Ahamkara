import { serve } from "@hono/node-server";
import { serveStatic } from "@hono/node-server/serve-static";
import { Hono } from "hono";
import { cors } from "hono/cors";

const app = new Hono();

app.use("/api/*", cors());

app.get("/api/health", (c) =>
  c.json({ status: "ok", timestamp: new Date().toISOString() }),
);

app.use(
  "/*",
  serveStatic({
    root: "./dist",
    index: "index.html",
  }),
);

const port = Number(process.env.PORT) || 3000;

if (process.env.NODE_ENV !== "test") {
  console.log(`Server running on http://localhost:${port}`);
  serve({ fetch: app.fetch, port });
}

export default app;
