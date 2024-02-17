import { defineConfig } from 'vite'

export default defineConfig({
  optimizeDeps: {
    include: ['@elemaudio/core'],
    force: true,
  },
  test: {
    browser: {
      enabled: true,
      name: 'firefox', // There's a bug with webdriver downloading chrome right now
      headless: true,
    },
  },
})
