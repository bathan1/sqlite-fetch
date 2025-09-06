import { fileURLToPath } from "node:url";
import { defineConfig } from "vitest/config";

export default defineConfig({
    resolve: {
        alias: {
            "~": fileURLToPath(new URL("./src", import.meta.url)),
            "@": fileURLToPath(new URL("./docs", import.meta.url)),
        },
    },
    test: {
        coverage: {
            provider: "istanbul",
            include: ["src/**/*.ts"],
        },
        globals: true,
        projects: [
            {
                test: {
                    name: "native",
                    include: ["tests/**/*.test.ts", "tests/**/*.spec.ts"],
                    environment: "node",
                    globals: true
                }
            },
            {
                extends: true,
                test: {
                    name: "web",
                    include: ["src/**/*.test.ts"],
                    globals: true,
                    browser: {
                        enabled: true,
                        provider: "preview",
                        instances: [
                            {
                                browser: "firefox",
                            },
                            {
                                browser: "chromium"
                            }
                        ],
                    },
                },
            },
        ],
    },
    optimizeDeps: {
        exclude: ["@sqlite.org/sqlite-wasm"],
    },
    // for SharedArrayBuffer access
    server: {
        headers: {
            "Cross-Origin-Opener-Policy": "same-origin",
            "Cross-Origin-Embedder-Policy": "require-corp",
        },
    },
});
