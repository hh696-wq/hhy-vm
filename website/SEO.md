# HHY website search and AI discovery

The site implements bilingual technical SEO in the application itself:

- unique Chinese and English titles, descriptions, and query vocabulary;
- self-referencing canonical URLs plus reciprocal `hreflang` alternatives;
- Open Graph and Twitter cards;
- `WebSite`, `SoftwareApplication`, `CollectionPage`, and `TechArticle` JSON-LD;
- crawlable static HTML for every Learn chapter;
- `robots.txt`, `sitemap.xml`, and `manifest.webmanifest`;
- explicit access for Googlebot, Bingbot, OAI-SearchBot, ChatGPT-User, and GPTBot.

Metadata describes only capabilities implemented by HHY 1.1.2. Do not add
keyword-stuffed pages, fake ratings, fake reviews, unsupported platforms, or
structured data that is not represented by visible page content.

## Search engine verification

Set verification values before the production build:

```sh
export NEXT_PUBLIC_GOOGLE_SITE_VERIFICATION="value-from-google-search-console"
export NEXT_PUBLIC_BING_SITE_VERIFICATION="value-from-bing-webmaster-tools"
npm run check
```

After `https://hhylang.dev` is live:

1. Add the domain property to Google Search Console and Bing Webmaster Tools.
2. Submit `https://hhylang.dev/sitemap.xml` to both services.
3. Inspect `/zh`, `/en`, and one Learn chapter with each URL inspection tool.
4. Confirm the reverse proxy does not block Googlebot, Bingbot, or OAI-SearchBot.
5. Use Bing IndexNow after deployments if faster update discovery is needed.

## Measurement

Track impressions, indexed pages, query language, click-through rate, and links
to Learn chapters. AI-search traffic should be evaluated through Search Console
and referrers rather than through a separate, invented “GEO score.”
