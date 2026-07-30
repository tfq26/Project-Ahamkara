<template>
  <div class="home">
    <h1>Ahamkara</h1>
    <p class="lead">
      A custom C++20 game engine and multiplayer tech demo built from scratch.
    </p>
    <p>
      Dedicated authoritative server with client networking skeleton —
      UDP transport at ~60 Hz, GLFW3 debug renderer, CMake + Ninja build.
    </p>
    <div class="links">
      <a href="https://git.2helix.org/taufeeq26/Project-Ahamkara">Source</a>
    </div>
  </div>
</template>

<style scoped>
.home {
  text-align: center;
  padding-top: 2rem;
}
h1 {
  font-size: 2.5rem;
  margin: 0 0 0.5rem;
  background: linear-gradient(135deg, #58a6ff, #bc8cff);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
}
.lead {
  font-size: 1.1rem;
}
p {
  line-height: 1.6;
  color: #8b949e;
}
.links {
  margin-top: 2rem;
}
.links a {
  color: #58a6ff;
  text-decoration: none;
}
.links a:hover {
  text-decoration: underline;
}
</style>
