<template>
  <main>
    <header class="hero">
      <h1>Ahamkara</h1>
      <p class="hero__tagline">A custom C++20 game engine and multiplayer tech demo built from scratch.</p>
    </header>

    <!-- Section: What is Wish? -->
    <section class="section" id="wish">
      <h2>What is Wish?</h2>

      <p class="section__lead">
        Wish is the independent backend and live-service platform for games built
        with Ahamkara. It owns the online services a modern multiplayer game
        needs — identity and authentication, sessions and admission, parties,
        matchmaking, activity lifecycle, replication envelopes, administration,
        and backend integrations such as Nakama — all as game-neutral services
        that any game can use.
      </p>

      <p>
        Wish is deliberately separate from the engine. Ahamkara is a reusable
        C++20 engine that focuses on rendering, simulation, physics, animation,
        audio, and deterministic networking. Wish focuses on everything that
        happens <em>around</em> the game: who is playing, which session they are
        in, what activity they joined, and how game state is replicated and
        administered. Because Wish builds, tests, and packages without Ahamkara,
        it can power a single-player title, an online multiplayer game, or any
        other project that needs sessions and live services.
      </p>

      <p>
        The two connect through a versioned, game-neutral protocol. A game built
        on Ahamkara sends identity and session requests to Wish and consumes its
        activity and replication services, while a translation layer (owned by
        the shipping game, Flashback) converts between Wish session data and
        Ahamkara game state. The result: the engine stays clean and reusable,
        the live-service layer stays independent, and together they let a single
        developer ship a modern networked game.
      </p>

      <!-- Simple diagram of the relationship between Ahamkara and Wish. -->
      <figure class="diagram" aria-label="Diagram showing how Ahamkara and Wish connect">
        <svg viewBox="0 0 720 180" role="img" aria-labelledby="wish-diagram-title" xmlns="http://www.w3.org/2000/svg">
          <title id="wish-diagram-title">How Ahamkara and Wish connect</title>
          <desc>A game built on the Ahamkara engine talks to the Wish live-service platform through a versioned, game-neutral protocol.</desc>

          <g font-family="system-ui, sans-serif" font-size="13" text-anchor="middle">
            <!-- Ahamkara box -->
            <rect x="30" y="30" width="210" height="120" rx="10" fill="#1f2937" stroke="#374151" stroke-width="2"/>
            <text x="135" y="58" fill="#ffffff" font-weight="bold">Ahamkara</text>
            <text x="135" y="80" fill="#d1d5db">C++20 game engine</text>
            <text x="135" y="100" fill="#d1d5db">rendering · simulation</text>
            <text x="135" y="118" fill="#d1d5db">networking · physics</text>
            <text x="135" y="136" fill="#d1d5db">animation · audio</text>

            <!-- Protocol arrow -->
            <line x1="240" y1="90" x2="300" y2="90" stroke="#6ee7b7" stroke-width="3"/>
            <polygon points="300,82 318,90 300,98" fill="#6ee7b7"/>
            <text x="285" y="72" fill="#9ca3af" font-size="12">versioned</text>
            <text x="285" y="128" fill="#9ca3af" font-size="12">protocol</text>

            <!-- Wish box -->
            <rect x="320" y="30" width="210" height="120" rx="10" fill="#064e3b" stroke="#10b981" stroke-width="2"/>
            <text x="425" y="58" fill="#ffffff" font-weight="bold">Wish</text>
            <text x="425" y="80" fill="#d1fae5">live-service platform</text>
            <text x="425" y="100" fill="#d1fae5">identity · sessions</text>
            <text x="425" y="118" fill="#d1fae5">activities · replication</text>
            <text x="425" y="136" fill="#d1fae5">backend integrations</text>

            <!-- Game box -->
            <rect x="530" y="30" width="170" height="120" rx="10" fill="#312e81" stroke="#818cf8" stroke-width="2"/>
            <text x="615" y="58" fill="#ffffff" font-weight="bold">Your Game</text>
            <text x="615" y="80" fill="#e0e7ff">built on Ahamkara</text>
            <text x="615" y="100" fill="#e0e7ff">uses Wish for its</text>
            <text x="615" y="118" fill="#e0e7ff">online features</text>
          </g>
        </svg>
        <figcaption>Ahamkara powers the game; Wish powers the live-service layer around it.</figcaption>
      </figure>
    </section>
  </main>
</template>

<style>
body {
  margin: 0;
  font-family: system-ui, sans-serif;
  background: #0f172a;
  color: #e2e8f0;
  line-height: 1.6;
}

main {
  max-width: 860px;
  margin: 0 auto;
  padding: 2rem 1.25rem;
}

.hero {
  text-align: center;
  padding: 3rem 0 2rem;
}

.hero h1 {
  margin: 0 0 0.5rem;
  font-size: 2.5rem;
  color: #ffffff;
}

.hero__tagline {
  margin: 0;
  font-size: 1.1rem;
  color: #94a3b8;
}

.section {
  margin-top: 2.5rem;
}

.section h2 {
  font-size: 1.75rem;
  color: #ffffff;
  border-bottom: 2px solid #10b981;
  padding-bottom: 0.5rem;
}

.section__lead {
  font-size: 1.1rem;
}

.section p {
  margin: 1rem 0;
}

.diagram {
  margin: 2rem 0 0;
  text-align: center;
}

.diagram svg {
  max-width: 100%;
  height: auto;
}

.diagram figcaption {
  font-size: 0.9rem;
  color: #94a3b8;
  margin-top: 0.5rem;
}
</style>
