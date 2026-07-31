<template>
  <section class="page-section">
    <div class="section-inner">
      <h2 class="section-label">01 — The Engine</h2>
      <h3 class="section-title">What is Ahamkara?</h3>
      <div class="content-grid">
        <div class="content-text">
          <p>
            Ahamkara is a <strong>custom C++20 game engine and multiplayer tech demo</strong>
            built from scratch. It provides the foundational systems needed to build modern
            networked games: core utilities, a dedicated authoritative server, real-time
            networking, 3D rendering, physics/collision, animation, audio, and a platform
            abstraction layer.
          </p>
          <p>
            The engine follows a clean layered architecture — core libraries ship as versioned
            CMake packages, while game-specific logic lives in Flashback, the product layer.
            Ahamkara owns the reusable technology; Flashback owns the shipped experience.
          </p>
          <ul class="feature-list">
            <li><span class="feature-icon">&gt;</span> Authoritative server with UDP networking at ~60 Hz</li>
            <li><span class="feature-icon">&gt;</span> GLFW3 native window + input layer with controller support</li>
            <li><span class="feature-icon">&gt;</span> 3D debug renderer with ground grid, axes, and player marker</li>
            <li><span class="feature-icon">&gt;</span> Modular engine libraries: core, network, runtime, platform, render, UI, audio</li>
          </ul>
          <router-link to="/" class="back-link">&larr; Back to Home</router-link>
        </div>
        <div class="content-image placeholder-img">
          <div class="placeholder-box">Engine Preview</div>
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
