# sqlite-fetch
HTTP over SQLite.

| OS      | Arch  | Tested |
|---------|-------|--------|
| Linux   | x64   | ✅      |
| Linux   | arm64 | ❌      |
| macOS   | x64   | ❌      |
| macOS   | arm64 | ❌      |
| Windows | x64   | ✅      |
| Windows | arm64 | ❌      |

## TLDR
SQLite C API headers are from the SQLite [3.38.0](https://www.sqlite.org/releaselog/3_38_0.html) release.

CMake Flags:
| Flag                | Description                                                                  | Default   |
|---------------------|------------------------------------------------------------------------------|-----------|
| `BUILD_FETCH`       | Build main extension (`libfetch.*`).                                         | `ON`      |
| `BUILD_EXT`         | Build SQLite extensions (e.g., `csv`, `uuid`).                               | `OFF`     |
| `THIRDPARTY_STATIC` | Link `libuv`/`libcurl` statically to `libfetch` for a (more) standalone lib. | `OFF`     |
| `SQLITE_VERSION`    | SQLite source version to fetch (headers-only usage).                         | `3380000` |
| `SQLITE_YEAR`       | Release year used in the SQLite source URL.                                  | `2022`    |

1. Dynamically link deps to main build
```bash
pnpm run cmake:dynamic
```
2. Statically link deps to main build
```bash
pnpm run cmake:static
```
3. Compile SQLite `/ext` extensions are not included in SQLite by default.
```bash
pnpm run cmake:ext <extension-target1> <extension-target2> ...
```
4. Build npm release
```bash
pnpm run build
```

## Main Build
Building from source on your local machine is easy.

You just need to install [libcurl](https://curl.se/libcurl/).

Once you have that, [compile the extension](https://www.sqlite.org/loadext.html). 
Using [`gcc`](https://gcc.gnu.org/) for example:

```bash
# omit include flags for brevity
gcc src/fetch.c -g -fPIC -shared -lcurl
```

And that's it!

You can also try the [`CMakeLists`](./CMakeLists.txt) if you have cmake.
The 4 commands to build the extension are:

```bash
mkdir build && cd build && cmake .. && make fetch
```

The default build flags link `libcurl` dynamically.

Set the `THIRDPARTY_STATIC` build variable to `ON` to tell CMake 
to link the dependencies *statically*.

```bash
mkdir build && cd build && cmake .. -DTHIRDPARTY_STATIC=ON && make fetch
```

However you go about it, the final output is the single shareable library file.
Releases follow the naming convention `"libfetch.${OS}-${ARCH}.${EXT}"` based on your
system, which the CMake script follows.

You can open your SQLite3 shell to see if it compiled successfully:

```bash
# make sure you're in `sqlite-fetch` directory (where this README is)
sqlite3 
```

```sql
.load ./build/libfetch.linux-x64
select json, ok from fetch('https://jsonplaceholder.typicode.com/todos')
```

## Tests
[vitest](https://vitest.dev/) test scripts are under [`/tests`](). File conventions are:

1. `*.spec.ts` -> SQL logic
2. `*.test.ts` -> System logic

## CMake Extras
The Makefile generated from cmake also targets the SQLite `/ext` extensions that are
not included into the amalgamation by default. Extension versions are from the SQLite source that cmake
pulls to link the headers, so they're pulled directly from whatever `SQLITE_VERSION` and `SQLITE_YEAR` 
flags are set to.

To automate these builds, run `cmake` with the `BUILD_EXT` flag set to `ON`:

```bash
# from inside build
cmake .. -DBUILD_EXT=ON && make <ext-target>
```

Extra targets include:
1. [`csv`](https://sqlite.org/src/file?name=ext/misc/csv.c&ci=tip)
2. [`uuid`](https://sqlite.org/src/file?name=ext/misc/uuid.c&ci=tip)

For the full list, refer to the [`CMakeLists`](./CMakeLists.txt).

## npm release
This project has a [npm](npmjs.com) distribution to make integration into Javascript
apps easier. The main module is from [`src/index.ts`](./src/index.ts),
that exports the paths to the compiled extension(s), and the modules are compiled
using [`rollup`](https://rollupjs.org/) with [`vite`](https://vite.dev/guide/build.html#library-mode):

```bash
npx vite build
```

This does **not** handle compiling the extensions, so you are in charge of providing
the lib files. It expects the binaries to be directly next to the source file (`/dist/index.js`)
with the extensions being named like this:

```js
const FETCH = `libfetch.${platform}-${arch}`;
const CSV = `libcsv.${platform}-${arch}`;
const UUID = `libuuid.${platform}-${arch}`;
```

The `build` script inside `package.json` is an alias for the `vite build` invocation, and will also trigger
the `postbuild` script that runs the cmake static build script of the main extension + copies it over to the `/dist`
folder.

It also ships a node.js script at `/bin/cli.mjs` which automates building the extra extensions with cmake,
making the extra extension path exports point to library files that exist.

```bash
# builds csv
npx sqlite-fetch csv 
# builds uuid
npx sqlite-fetch uuid
# builds csv AND uuid
npx sqlite-fetch csv uuid
# remove all extension builds
npx sqlite-fetch clean
```
