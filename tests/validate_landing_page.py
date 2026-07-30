#!/usr/bin/env python3
"""Validate the marketing landing page exists and contains expected content."""
import sys, re, os

SITE_DIR = os.path.join(os.path.dirname(__file__), '..', 'site')
INDEX = os.path.join(SITE_DIR, 'index.html')
LOGO = os.path.join(SITE_DIR, 'assets', 'logo.svg')

checks = []
ok = True

# File existence
checks.append(('index.html exists', os.path.isfile(INDEX)))
checks.append(('logo.svg exists', os.path.isfile(LOGO)))

if os.path.isfile(INDEX):
    html = open(INDEX).read()
    checks.append(('Vue.js CDN loaded', 'unpkg.com/vue@3' in html))
    checks.append(('Responsive viewport meta', 'viewport' in html))
    checks.append(('Section: Ahamkara', bool(re.search(r'What is.*Ahamkara', html))))
    checks.append(('Section: Wish', bool(re.search(r'Introducing.*Wish', html))))
    checks.append(('Section: Inspiration', bool(re.search(r'Born from', html))))
    checks.append(('Vue app mount', 'createApp' in html and '.mount' in html))
    checks.append(('Stars background effect', 'stars' in html))
    checks.append(('Card grid layout', 'card-grid' in html))
    checks.append(('Has hero section', 'hero' in html))
    checks.append(('Has scroll hint', 'scroll-hint' in html))
    checks.append(('Responsive media query', '@media' in html))

print(f'Landing page validation ({len(checks)} checks)')
print('=' * 45)
for name, passed in checks:
    status = 'PASS' if passed else 'FAIL'
    if not passed:
        ok = False
    print(f'  [{status}] {name}')

print()
if ok:
    print('All checks passed.')
    sys.exit(0)
else:
    print('Some checks FAILED. Review the page before merging.')
    sys.exit(1)
