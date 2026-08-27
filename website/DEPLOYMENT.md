# HHY Language website deployment

The production site uses the Next.js standalone output and PM2. Build on the
Linux server so native dependencies and generated files match the target host.

## Release archive

The production archive contains source code and lockfiles, but excludes
`node_modules`, `.next`, local caches, screenshots, and macOS metadata. Build on
the Linux server so generated files match the target host.

## First deployment

```sh
unzip -o hhylang.dev-v1.1.1_YYYYMMDD.zip
cd hhylang-website
npm ci
npm run build
pm2 start ecosystem.config.cjs
pm2 save
```

## Update

```sh
unzip -o hhylang.dev-v1.1.1_YYYYMMDD.zip
cd hhylang-website
npm ci
npm run build
pm2 restart hhylang-website --update-env
pm2 save
```

`npm run build` automatically copies `public` and `.next/static` into the
standalone runtime. PM2 listens only on `127.0.0.1:8100`; expose the site through
Nginx or another TLS reverse proxy.

## Verify

```sh
pm2 status
pm2 logs hhylang-website --lines 50
curl -I http://127.0.0.1:8100/zh
curl -I http://127.0.0.1:8100/en
curl -I http://127.0.0.1:8100/zh/learn/quick-start
```

Terminate TLS at Nginx or another reverse proxy and forward requests to
`127.0.0.1:8100`. Do not expose the PM2 application port directly.

Set the optional Google Search Console and Bing Webmaster Tools verification
environment variables before `npm run check`; see [SEO.md](SEO.md). The public
reverse proxy must serve `/robots.txt`, `/sitemap.xml`, and
`/manifest.webmanifest` without authentication or bot challenges.
