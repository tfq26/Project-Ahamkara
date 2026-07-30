<template>
  <section class="hero">
    <div class="hero-overlay"></div>
    <div class="hero-content">
      <span class="hero-badge">C++20 Game Engine</span>
      <h1 class="hero-title">AHAMKARA</h1>
      <p class="hero-subtitle">Build. Connect. Dominate.</p>
      <p class="hero-desc">
        A custom game engine and multiplayer tech demo built from the ground up.
        Powered by C++20, inspired by the greats.
      </p>
      <div class="hero-cta">
        <router-link to="/engine" class="btn btn-primary">Explore the Engine</router-link>
        <a href="https://github.com/tfq26/Project-Ahamkara" class="btn btn-outline">View on GitHub</a>
      </div>
    </div>
  </section>
</template>

<style scoped>
.hero {
  position: relative; min-height: 100vh; display: flex;
  align-items: center; justify-content: center;
  background: linear-gradient(135deg, #0a0a0a 0%, #1a1a2e 50%, #0a0a0a 100%);
  text-align: center; padding: 6rem 2rem 2rem; overflow: hidden;
}
.hero-overlay {
  position: absolute; inset: 0;
  background: radial-gradient(ellipse at center, transparent 0%, rgba(0,0,0,0.6) 100%);
}
.hero-content { position: relative; z-index: 2; max-width: 800px; }
.hero-badge {
  display: inline-block; padding: 0.4rem 1.2rem; margin-bottom: 1.5rem;
  border: 1px solid rgba(240,165,0,0.5); border-radius: 4px;
  font-family: 'Oswald', 'Impact', sans-serif;
  font-size: 0.85rem; letter-spacing: 0.2em;
  color: #f0a500; text-transform: uppercase;
}
.hero-title {
  font-family: 'Oswald', 'Impact', sans-serif;
  font-size: clamp(3.5rem, 12vw, 8rem);
  font-weight: 700; line-height: 1; letter-spacing: 0.08em;
  background: linear-gradient(180deg, #fff 0%, #f0a500 100%);
  -webkit-background-clip: text; -webkit-text-fill-color: transparent;
  background-clip: text; margin-bottom: 0.5rem;
}
.hero-subtitle {
  font-size: clamp(1.2rem, 3vw, 1.8rem);
  font-weight: 300; color: #f0a500; margin-bottom: 1rem;
}
.hero-desc { color: #aaa; max-width: 560px; margin: 0 auto 2rem; font-size: 1.05rem; }
.hero-cta { display: flex; gap: 1rem; justify-content: center; flex-wrap: wrap; }
.btn {
  display: inline-block; padding: 0.85rem 2rem; border-radius: 6px;
  font-weight: 600; font-size: 0.95rem; transition: all 0.3s;
  text-transform: uppercase; letter-spacing: 0.05em;
}
.btn-primary {
  background: #f0a500; color: #0a0a0a; border: 2px solid #f0a500;
}
.btn-primary:hover { background: #ffc107; border-color: #ffc107; color: #0a0a0a; }
.btn-outline {
  border: 2px solid rgba(255,255,255,0.3); color: #e8e8e8;
}
.btn-outline:hover { border-color: #f0a500; color: #f0a500; }
</style>
