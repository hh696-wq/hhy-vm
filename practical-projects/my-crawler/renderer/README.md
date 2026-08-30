# Optional JavaScript renderer

The crawler can render authorized HTTP(S) pages with Playwright while keeping the Lexbor `html` extension side-effect free.

```sh
cd practical-projects/my-crawler/renderer
npm ci
npx playwright install chromium
```

Set `javascript_rendering` to `true` and keep `renderer_script` pointed at `render.mjs`. Every browser request, including redirects and subresources, must remain within `allowed_domains`; DNS results are rejected when they resolve to private, loopback or link-local addresses unless the crawler explicitly enables private networks.
