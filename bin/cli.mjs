#!/usr/bin/env node
import { existsSync, readdirSync } from "node:fs";
import { rm, mkdir, copyFile, readdir } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { execa } from "execa";
import ora from "ora";
import { csv, uuid } from "sqlite-fetch";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(__dirname, "..");
const BUILD = path.join(ROOT, "build");
const DIST = path.join(ROOT, "dist");

/**
 * @param {string} baseName extension name
 * @returns {Promise<string[]>} matched files that were (presumably) removed
 */
export async function removeMatchingByBase(baseName) {
    const files = await readdir(DIST);
    const matching = files.filter((f) => f.startsWith(baseName));
    for (const file of matching) {
        const fullPath = path.join(DIST, file);
        await rm(fullPath, { force: true });
    }
    return matching;
}

const escapeRe = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");

function buildArtifactMatcher(targets) {
    const union = targets.map(escapeRe).join("|");
    // lib<name>.<platform-arch>.(so|dylib|dll)
    return new RegExp(`^lib(?:${union})\\..+\\.(?:so|dylib|dll)$`, "i");
}

function listArtifacts(matcher) {
    const candidates = [
        BUILD,
        path.join(BUILD, "Release"),
        path.join(BUILD, "Debug"),
    ].filter(existsSync);

    const out = [];
    for (const dir of candidates) {
        const names = readdirSync(dir);
        for (const name of names) {
            if (matcher.test(name)) out.push(path.join(dir, name));
        }
    }
    return out;
}

/**
 * Builds the requested extensions included as a target from the cmake build
 *
 * @param {Object} opts Options object.
 * @param {string[]} [opts.buildTargets=[]] Names of the build targets to compile.
 * @param {string[]} [opts.jobs=[]] Extra job flags to pass to CMake.
 * @returns {Promise<void>}
 */
export async function buildExtensions({ buildTargets = [], jobs = [] }) {
    const spinner = ora("Preparing build...").start();

    try {
        if (buildTargets.includes("clean")) {
            spinner.text = "Target 'clean' found, removing extra extensions...";
            const removedExtensions = [];

            for (const path of [csv, uuid]) {
                const baseName = path.split("/").pop();
                if (baseName) {
                    const matches = await removeMatchingByBase(baseName);
                    if (matches.length > 0) {
                        spinner.text = `removing ${path}...`;
                        removedExtensions.push(baseName);
                    }
                }
            }

            spinner.succeed(
                removedExtensions.length
                    ? `removed extra targets: ${removedExtensions.map((v) => `"${v}"`).join(", ")}`
                    : "no extra targets to remove",
            );
            return;
        }
        // fresh build dir
        spinner.text = "Cleaning build directory";
        await rm(BUILD, { recursive: true, force: true });
        await mkdir(BUILD, { recursive: true });

        // ensure dist exists for copies later
        await mkdir(DIST, { recursive: true });

        // configure
        spinner.text = "Configuring CMake";
        await execa(
            "cmake",
            [
                ROOT,
                "-DBUILD_FETCH=OFF",
                "-DBUILD_EXT=ON",
                "-DCMAKE_BUILD_TYPE=Release",
            ],
            { cwd: BUILD, stdio: "inherit" },
        );

        // build
        spinner.text = `Building targets: ${buildTargets.join(", ")}`;
        await execa(
            "cmake",
            [
                "--build",
                ".",
                "--target",
                ...buildTargets,
                "--config",
                "Release",
                ...jobs,
            ],
            { cwd: BUILD, stdio: "inherit" },
        );

        // collect artifacts
        spinner.text = "Collecting artifacts";
        const matcher = buildArtifactMatcher(buildTargets);
        const artifacts = listArtifacts(matcher);

        // verify each requested target produced something
        const missing = buildTargets.filter(
            (t) =>
                !artifacts.some((p) => path.basename(p).startsWith(`lib${t}.`)),
        );
        if (missing.length) {
            throw new Error(
                `Build finished but missing expected artifacts for: ${missing.join(", ")}`,
            );
        }

        // copy artifacts
        for (const src of artifacts) {
            const dest = path.join(DIST, path.basename(src));
            await copyFile(src, dest);
            ora().succeed(`Copied ${path.relative(ROOT, dest)}`);
        }

        spinner.succeed(`Build complete for: ${buildTargets.join(", ")}`);
    } catch (err) {
        spinner.fail("Build failed");
        ora().fail(String(err?.message ?? err));
        throw err;
    }
}

const thisFile = fileURLToPath(import.meta.url);
const argv1Real = path.resolve(process.argv[1] || "");

if (argv1Real === thisFile || argv1Real.endsWith("/.bin/sqlite-fetch")) {
  // main CLI execution
  const args = process.argv.slice(2);
  const buildTargets = args.length ? args : ["csv", "uuid"];
  await buildExtensions({ buildTargets });
}