import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import articles from './vite-plugin-articles.js'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react(), articles()],
  define: {
    __BUILD_UTC__: JSON.stringify(new Date().toISOString()),
  },
  server: {
    proxy: {
      '/api': 'http://localhost:3002',
      '/avatars': 'http://localhost:3002',
      '/articles.generated.json': 'http://localhost:3002',
      '/news.generated.json': 'http://localhost:3002',
    },
  },
})
