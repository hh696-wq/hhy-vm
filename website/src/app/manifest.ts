import type { MetadataRoute } from "next";

export default function manifest(): MetadataRoute.Manifest {
  return {
    name: "HHY Language",
    short_name: "HHY",
    description: "A flow-first scripting language for system automation.",
    start_url: "/en",
    display: "standalone",
    background_color: "#ffffff",
    theme_color: "#146df5",
    icons: [{ src: "/hhy-logo.png", sizes: "1254x1254", type: "image/png" }]
  };
}
