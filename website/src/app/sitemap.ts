import type { MetadataRoute } from "next";
import { chapters } from "@/lib/docs";

const origin = "https://hhylang.dev";

export default function sitemap(): MetadataRoute.Sitemap {
  const pages = ["", "/learn", ...chapters.map((chapter) => `/learn/${chapter.slug}`)];

  return (["zh", "en"] as const).flatMap((language) =>
    pages.map((page) => ({
      url: `${origin}/${language}${page}`,
      changeFrequency: page === "" ? "weekly" as const : "monthly" as const,
      priority: page === "" ? 1 : page === "/learn" ? 0.9 : 0.8,
      alternates: {
        languages: {
          zh: `${origin}/zh${page}`,
          en: `${origin}/en${page}`
        }
      }
    }))
  );
}
