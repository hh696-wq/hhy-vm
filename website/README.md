# HHY Language website

Bilingual Next.js website for [hhylang.dev](https://hhylang.dev).

```sh
npm install
npm run dev -- --hostname 127.0.0.1 --port 8100
```

Open `/zh` for Chinese or `/en` for English. Run `npm run check` before a
production deployment. See [DEPLOYMENT.md](DEPLOYMENT.md) for PM2 deployment.
Search metadata and post-deployment indexing steps are documented in
[SEO.md](SEO.md).

## Printable manual and PDF

The bilingual print editions use the same `src/lib/docs.ts` content as the website:

```sh
npm run pdf:zh
npm run pdf:en
```

Each command creates a production build, starts a temporary local server, and uses an installed Chrome, Edge, or Chromium browser. Final PDFs are written to `output/pdf/`. The browser print pages are `/zh/learn/print` and `/en/learn/print`.
