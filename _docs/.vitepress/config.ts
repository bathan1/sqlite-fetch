import { defineConfig } from "vitepress";
import { fileURLToPath } from "url";
import typedocSidebar from '../api/typedoc-sidebar.json';

// https://vitepress.dev/reference/site-config
export default defineConfig({
  title: "SQLite Fetch",
  description: "Fetch and then some more in SQLite",
  head: [
    ["link", { rel: "icon", type: "image/svg+xml", href: "/logo-dark.svg" }],
  ],
  themeConfig: {
    // https://vitepress.dev/reference/default-theme-config
    nav: [
      { text: "API", link: "/api/" },
    ],
    logo: {
      light: "logo.svg",
      dark: "logo-dark.svg",
    },
    footer: {
      copyright: "Copyright © 2025-present Nathan Oh",
      message: "Released under MIT license",
    },
    sidebar: [
      {
        text: "Guide",
        items: [
          {
            text: "Introduction",
            link: "/guide/introduction",
          },
          {
            text: "API Design",
            link: "/guide/api-design",
          }
        ],
      },
      {
        text: 'API',
        items: typedocSidebar,
      },
    ],

    socialLinks: [
      { icon: "github", link: "https://github.com/nestorhealth/medfetch.js" },
    ],
  },
  ignoreDeadLinks: true,
  vite: {
    plugins: [
      // @ts-expect-error lol
      {
        configureServer(server) {
          server.middlewares.use((_req, res, next) => {
            res.setHeader("Cross-Origin-Opener-Policy", "same-origin");
            res.setHeader("Cross-Origin-Embedder-Policy", "require-corp");
            next();
          });
        },
      },
    ],
    resolve: {
      alias: {
        "~": fileURLToPath(new URL("../../src", import.meta.url)),
        "@": fileURLToPath(new URL("..", import.meta.url)),
      },
    },
    optimizeDeps: {
      exclude: ["@sqlite.org/sqlite-wasm"],
    },
    envDir: fileURLToPath(new URL("../", import.meta.url)),
  },
});
