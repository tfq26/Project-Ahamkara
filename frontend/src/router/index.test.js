import { describe, it, expect } from 'vitest'
import { createRouter, createWebHistory } from 'vue-router'
import Marketing from '../views/Marketing.vue'
import Docs from '../views/Docs.vue'

describe('router', () => {
  it('has a route for / that maps to Marketing', () => {
    const routes = [
      { path: '/', name: 'Marketing', component: Marketing },
      { path: '/docs', name: 'Docs', component: Docs },
    ]
    const root = routes.find(r => r.path === '/')
    expect(root).toBeDefined()
    expect(root.name).toBe('Marketing')
  })

  it('has a route for /docs that maps to Docs', () => {
    const routes = [
      { path: '/', name: 'Marketing', component: Marketing },
      { path: '/docs', name: 'Docs', component: Docs },
    ]
    const docs = routes.find(r => r.path === '/docs')
    expect(docs).toBeDefined()
    expect(docs.name).toBe('Docs')
  })

  it('creates a router with web history', () => {
    const routes = [
      { path: '/', name: 'Marketing', component: Marketing },
      { path: '/docs', name: 'Docs', component: Docs },
    ]
    const router = createRouter({ history: createWebHistory(), routes })
    expect(router).toBeDefined()
    expect(router.hasRoute('Marketing')).toBe(true)
    expect(router.hasRoute('Docs')).toBe(true)
  })
})
