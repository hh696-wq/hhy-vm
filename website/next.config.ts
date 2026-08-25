import type { NextConfig } from "next";

const nextConfig: NextConfig = {
  output: "standalone",
  async redirects() {
    return [
      {
        source: "/",
        destination: "/zh",
        permanent: false
      }
    ];
  }
};

export default nextConfig;
