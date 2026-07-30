import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

export default defineConfig({
  plugins: [vue()],
  // Root-relative base — adjust if deploying to a subdirectory
  base: '/',
  build: {
    outDir: 'dist',
    emptyOutDir: true,
  },
})
