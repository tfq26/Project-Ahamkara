<template>
  <nav class="navbar">
    <div class="nav-inner">
      <router-link to="/" class="logo">AHAMKARA</router-link>
      <ul class="nav-links">
        <li><router-link to="/engine">Engine</router-link></li>
        <li><router-link to="/wish">Wish</router-link></li>
        <li><a href="https://github.com/tfq26/Project-Ahamkara">GitHub</a></li>
      </ul>
    </div>
  </nav>
</template>

<style scoped>
.navbar {
  position: fixed; top: 0; left: 0; right: 0; z-index: 100;
  background: rgba(10,10,10,0.85); backdrop-filter: blur(12px);
  border-bottom: 1px solid rgba(255,255,255,0.06);
}
.nav-inner {
  max-width: 1200px; margin: 0 auto; padding: 1rem 2rem;
  display: flex; justify-content: space-between; align-items: center;
}
.logo {
  font-family: 'Oswald', 'Impact', sans-serif;
  font-size: 1.5rem; font-weight: 700; letter-spacing: 0.15em; color: #fff;
}
.nav-links { list-style: none; display: flex; gap: 2rem; }
.nav-links a {
  color: #aaa; font-size: 0.9rem; font-weight: 500;
  text-transform: uppercase; letter-spacing: 0.08em;
}
.nav-links a:hover, .nav-links a.router-link-active { color: #f0a500; }
</style>
