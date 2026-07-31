import { createRouter, createWebHistory } from 'vue-router'
import Marketing from '../views/Marketing.vue'
import Docs from '../views/Docs.vue'

const routes = [
  { path: '/', name: 'Marketing', component: Marketing },
  { path: '/docs', name: 'Docs', component: Docs },
]

const router = createRouter({
  history: createWebHistory(),
  routes,
})

export default router
