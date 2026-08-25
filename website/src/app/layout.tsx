import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  metadataBase: new URL("https://hhylang.dev"),
  title: {
    default: "HHY Language — Pipe Everything",
    template: "%s · HHY Language"
  },
  description: "HHY is a flow-first scripting language for system automation, connecting files, processes, networks, and structured data through one pipeline model.",
  applicationName: "HHY Language",
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
