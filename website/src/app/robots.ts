import type { MetadataRoute } from "next";

export default function robots(): MetadataRoute.Robots {
  return {
    rules: [
      { userAgent: ["Googlebot", "Bingbot", "OAI-SearchBot", "ChatGPT-User", "GPTBot"], allow: "/" },
      { userAgent: "*", allow: "/" }
    ],
    sitemap: "https://hhylang.dev/sitemap.xml",
    host: "https://hhylang.dev"
  };
}
