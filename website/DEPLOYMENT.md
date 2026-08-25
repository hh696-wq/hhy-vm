# HHY Language website deployment

The production site uses the Next.js standalone output and PM2. Build on the
Linux server so native dependencies and generated files match the target host.

## First deployment

```sh
cd /home/hhylang/website
npm ci
npm run check
cp -R public .next/standalone/
cp -R .next/static .next/standalone/.next/
pm2 start ecosystem.config.cjs
pm2 save
```

## Update

```sh
cd /home/hhylang/website
npm ci
npm run check
cp -R public .next/standalone/
cp -R .next/static .next/standalone/.next/
pm2 restart hhylang-website --update-env
```

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
