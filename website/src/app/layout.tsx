import type { Metadata } from "next";
import { siteName, siteOrigin } from "@/lib/seo";
import "./globals.css";

const verification: Metadata["verification"] = {
  ...(process.env.NEXT_PUBLIC_GOOGLE_SITE_VERIFICATION
    ? { google: process.env.NEXT_PUBLIC_GOOGLE_SITE_VERIFICATION }
    : {}),
  ...(process.env.NEXT_PUBLIC_BING_SITE_VERIFICATION
    ? { other: { "msvalidate.01": process.env.NEXT_PUBLIC_BING_SITE_VERIFICATION } }
    : {})
};

export const metadata: Metadata = {
  metadataBase: new URL(siteOrigin),
  title: {
    default: "HHY Language — Pipe Everything",
    template: "%s · HHY Language"
  },
  description: "HHY is a flow-first scripting language for system automation, connecting files, processes, networks, and structured data through one pipeline model.",
  applicationName: "HHY Language",
  authors: [{ name: "HHY Language contributors", url: "https://github.com/hh696-wq/hhy-vm" }],
  creator: "HHY Language contributors",
  publisher: siteName,
  referrer: "origin-when-cross-origin",
  verification,
  robots: {
    index: true,
    follow: true,
    googleBot: {
      index: true,
      follow: true,
      "max-image-preview": "large",
      "max-snippet": -1,
      "max-video-preview": -1
    }
  },
  icons: { icon: "/hhy-logo.png", apple: "/hhy-logo.png" },
  openGraph: {
    type: "website",
    siteName: "HHY Language",
    title: "HHY Language — Pipe Everything",
    description: "A flow-first scripting language for system automation.",
    images: [{ url: "/hhy-logo.png", width: 1254, height: 1254, alt: "HHY Language" }]
  },
  twitter: {
    card: "summary",
    title: "HHY Language — Pipe Everything",
    description: "A flow-first scripting language for system automation.",
    images: ["/hhy-logo.png"]
  }
};

export default function RootLayout({ children }: Readonly<{ children: React.ReactNode }>) {
  return <html lang="zh-CN" data-scroll-behavior="smooth"><body>{children}</body></html>;
}
