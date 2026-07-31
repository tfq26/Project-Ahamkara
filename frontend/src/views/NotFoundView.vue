<template>
  <section class="not-found">
    <div class="not-found-content">
      <h1 class="nf-code">404</h1>
      <h2 class="nf-title">Page Not Found</h2>
      <p class="nf-desc">The page you're looking for doesn't exist or has been moved.</p>
      <router-link to="/" class="btn-home">&larr; Go Home</router-link>
    </div>
  </section>
</template>

<style scoped>
.not-found {
  min-height: 100vh; display: flex; align-items: center; justify-content: center;
  padding: 2rem; text-align: center;
}
.not-found-content { max-width: 480px; }
.nf-code {
  font-size: clamp(5rem, 15vw, 10rem); font-weight: 900;
  background: linear-gradient(180deg, #f0a500 0%, #8b6914 100%);
  -webkit-background-clip: text; -webkit-text-fill-color: transparent;
  background-clip: text; line-height: 1; margin-bottom: 1rem;
}
.nf-title { font-size: 1.8rem; color: #fff; margin-bottom: 0.5rem; }
.nf-desc { color: #8b949e; margin-bottom: 2rem; font-size: 1.05rem; }
.btn-home {
  display: inline-block; padding: 0.85rem 2rem; border-radius: 6px;
  background: #f0a500; color: #0a0a0a; font-weight: 600;
  text-transform: uppercase; letter-spacing: 0.05em; transition: all 0.3s;
}
.btn-home:hover { background: #ffc107; }
</style>
