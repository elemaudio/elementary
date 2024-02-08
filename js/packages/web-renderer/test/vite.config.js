import { defineConfig } from 'vite'

export default defineConfig({
  optimizeDeps: {
    include: ['@elemaudio/core'],
    force: true,
  },
  test: {
    browser: {
      enabled: true,
      name: 'chrome',
      headless: true,
    },
  },
})
