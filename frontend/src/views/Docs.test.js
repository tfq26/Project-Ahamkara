import { describe, it, expect } from 'vitest'
import { mount } from '@vue/test-utils'
import Docs from './Docs.vue'

describe('Docs', () => {
  it('renders the docs heading', () => {
    const wrapper = mount(Docs)
    expect(wrapper.find('h1').text()).toBe('Technical Documentation')
  })

  it('renders the description text', () => {
    const wrapper = mount(Docs)
    expect(wrapper.text()).toContain('Documentation and guides')
  })
})
