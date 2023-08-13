import { defineConfig } from 'vite'

export default defineConfig({
  optimizeDeps: {
    include: ['@elemaudio/core'],
  },
})
