import { readdirSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { defineConfig } from "vite";

export default defineConfig({
    resolve: {
        alias: {
            "@": fileURLToPath(new URL("./docs", import.meta.url)),
            "~": fileURLToPath(new URL("./src", import.meta.url)),
        },
    },
    server: {
        headers: {
            "Cross-Origin-Opener-Policy": "same-origin",
            "Cross-Origin-Embedder-Policy": "require-corp",
        },
    },
    optimizeDeps: {
        exclude: ["@sqlite.org/sqlite-wasm"],
    },
    build: {
        assetsInlineLimit: 0,
        minify: "terser",
        terserOptions: {
            format: {
                comments: "some",
            },
        },
        rollupOptions: {
            external: [
                "kysely",
                "@sqlite.org/sqlite-wasm",
                "@tanstack/react-query",
                "react",
                "react-dom",
                "node:worker_threads",
                "node:os",
                "node:url",
                "node:path",
                "worker_threads",
            ],
            output: {
                manualChunks: undefined,
                entryFileNames: `[name].js`,
                chunkFileNames: `[name]-[hash].js`,
                assetFileNames: `[name]-[hash].[ext]`
            },
        },
        lib: {
            entry: {
                "index": "./src/index.ts",
                "wasm": "./src/wasm.ts",
                "wasm.node": "./src/wasm.node.ts"
            },
            formats: ["es"],
            fileName: (format, name) =>
                format === "esm" || format === "es"
                    ? `${name}.mjs`
                    : `${name}.js`,
        },
    },
});
