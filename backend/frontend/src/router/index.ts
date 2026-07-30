import { createRouter, createWebHistory } from 'vue-router'
import Home from '@/views/Home.vue'
import Docs from '@/views/Docs.vue'

const routes = [
  { path: '/', component: Home },
  { path: '/docs', component: Docs },
]

export default createRouter({
  history: createWebHistory(),
  routes,
})
