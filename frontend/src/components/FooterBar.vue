<template>
  <footer class="footer">
    <div class="footer-inner">
      <span class="footer-logo">AHAMKARA</span>
      <p class="footer-text">
        Built with C++20 &mdash;
        Open source on <a href="https://github.com/tfq26/Project-Ahamkara">GitHub</a>
      </p>
    </div>
  </footer>
</template>

<style scoped>
.footer {
  border-top: 1px solid rgba(255,255,255,0.06);
  background: #050505;
}
.footer-inner {
  max-width: 1200px; margin: 0 auto; padding: 2rem;
  display: flex; justify-content: space-between; align-items: center;
  flex-wrap: wrap; gap: 1rem;
}
.footer-logo {
  font-family: 'Oswald', 'Impact', sans-serif;
  font-size: 1.2rem; font-weight: 700; letter-spacing: 0.15em; color: #555;
}
.footer-text { color: #555; font-size: 0.85rem; }
.footer-text a { color: #888; }
.footer-text a:hover { color: #f0a500; }
</style>
