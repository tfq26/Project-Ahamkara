<template>
  <section class="page-section">
    <div class="section-inner">
      <h2 class="section-label">02 — The Platform</h2>
      <h3 class="section-title">What is Wish?</h3>
      <div class="content-grid">
        <div class="content-text">
          <p>
            <strong>Wish</strong> is an independent backend, session, and activity platform
            that powers online services for games built with Ahamkara. It owns backend behavior
            without importing game-specific types — keeping a clean boundary between engine,
            game, and services.
          </p>
          <p>
            Wish provides session admission, activity routing, heartbeat services, metrics
            collection, player status tracking, and an HTTP admin server for live monitoring.
            Its protocol is game-neutral, designed to be consumed by any game client through
            a lightweight SDK.
          </p>
          <ul class="feature-list">
            <li><span class="feature-icon">&gt;</span> HTTP admin server with health, match, and player endpoints</li>
            <li><span class="feature-icon">&gt;</span> Heartbeat service for server discovery and liveness tracking</li>
            <li><span class="feature-icon">&gt;</span> Prometheus-compatible metrics collection</li>
            <li><span class="feature-icon">&gt;</span> Game-neutral protocol — Flashback owns the translation layer</li>
          </ul>
          <router-link to="/" class="back-link">&larr; Back to Home</router-link>
        </div>
        <div class="content-image placeholder-img">
          <div class="placeholder-box">Wish Platform</div>
        </div>
      </div>
    </div>
  </section>
</template>

<style scoped>
.page-section {
  min-height: 100vh; padding: 8rem 2rem 4rem;
  background: #0d1117;
}
.section-inner { max-width: 1100px; margin: 0 auto; }
.section-label {
  font-family: 'Oswald', 'Impact', sans-serif;
  font-size: 0.85rem; letter-spacing: 0.2em;
  color: #f0a500; text-transform: uppercase; margin-bottom: 0.5rem;
}
.section-title {
  font-size: clamp(1.8rem, 4vw, 2.5rem);
  font-weight: 700; margin-bottom: 2rem; color: #fff;
}
.content-grid {
  display: grid; grid-template-columns: 1fr 1fr; gap: 3rem; align-items: start;
}
@media (max-width: 768px) { .content-grid { grid-template-columns: 1fr; } }
.content-text p { color: #c9d1d9; margin-bottom: 1rem; font-size: 1.05rem; }
.feature-list { list-style: none; margin: 1.5rem 0; }
.feature-list li {
  padding: 0.5rem 0; color: #8b949e; border-bottom: 1px solid #21262d;
}
.feature-icon { color: #f0a500; font-weight: 700; margin-right: 0.5rem; }
.placeholder-img { display: flex; align-items: center; justify-content: center; }
.placeholder-box {
  width: 100%; aspect-ratio: 16/9; background: #161b22;
  border: 1px solid #30363d; border-radius: 8px;
  display: flex; align-items: center; justify-content: center;
  color: #484f58; font-size: 1.2rem;
}
.back-link {
  display: inline-block; margin-top: 1rem; color: #58a6ff;
}
.back-link:hover { color: #f0a500; }
</style>
