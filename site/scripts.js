// Smooth scroll for nav links, offset for fixed navbar
document.querySelectorAll('a[href^="#"]').forEach(anchor => {
  anchor.addEventListener('click', e => {
    const target = document.querySelector(anchor.getAttribute('href'));
    if (target) {
      e.preventDefault();
      const offset = 80;
      target.scrollIntoView({ behavior: 'smooth', block: 'start' });
      window.scrollBy(0, -offset);
    }
  });
});
