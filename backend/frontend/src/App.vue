<template>
  <div id="app-shell">
    <header>
      <nav>
        <router-link to="/">Home</router-link>
        <router-link to="/docs">Docs</router-link>
      </nav>
    </header>
    <main>
      <router-view />
    </main>
  </div>
</template>

<style>
:root {
  color-scheme: dark;
  font-family: system-ui, -apple-system, sans-serif;
}
body {
  margin: 0;
  min-height: 100vh;
  background: #0d1117;
  color: #c9d1d9;
}
#app-shell {
  max-width: 800px;
  margin: 0 auto;
  padding: 1rem;
}
header nav {
  display: flex;
  gap: 1.5rem;
  padding-bottom: 1rem;
  border-bottom: 1px solid #30363d;
  margin-bottom: 2rem;
}
header nav a {
  color: #8b949e;
  text-decoration: none;
  font-weight: 500;
}
header nav a.router-link-active {
  color: #58a6ff;
}
</style>
