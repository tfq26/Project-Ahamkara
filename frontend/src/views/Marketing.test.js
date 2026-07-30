import { describe, it, expect } from 'vitest'
import { mount } from '@vue/test-utils'
import Marketing from './Marketing.vue'

describe('Marketing', () => {
  it('renders the marketing heading', () => {
    const wrapper = mount(Marketing)
    expect(wrapper.find('h1').text()).toBe('Project Ahamkara')
  })

  it('renders the description text', () => {
    const wrapper = mount(Marketing)
    expect(wrapper.text()).toContain('high-performance game engine')
  })
})
