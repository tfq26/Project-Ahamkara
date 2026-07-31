<template>
  <div id="ahamkara-app">
    <NavBar />
    <main>
      <router-view />
    </main>
    <FooterBar />
  </div>
</template>

<script setup>
import NavBar from './components/NavBar.vue'
import FooterBar from './components/FooterBar.vue'
</script>

<style>
/* ── Reset & Base ── */
*, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
html { scroll-behavior: smooth; font-size: 16px; }
body {
  font-family: 'Inter', -apple-system, BlinkMacSystemFont, sans-serif;
  color: #e8e8e8; background: #0a0a0a; line-height: 1.6;
  -webkit-font-smoothing: antialiased;
}
img { max-width: 100%; height: auto; display: block; border-radius: 8px; }
a { color: #f0a500; text-decoration: none; transition: color 0.3s; }
a:hover { color: #ffc107; }
</style>
