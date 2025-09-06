# sqlite-fetch
Web APIs in SQLite.

| OS      | Arch  | Tested |
|---------|-------|--------|
| Linux   | x64   | ✅      |
| Linux   | arm64 | ❌      |
| macOS   | x64   | ❌      |
| macOS   | arm64 | ❌      |
| Windows | x64   | ✅      |
| Windows | arm64 | ❌      |

## TLDR
SQLite c api call headers are from the SQLite [3.38.0](https://www.sqlite.org/releaselog/3_38_0.html) release.

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

You just need [libcurl](https://curl.se/libcurl/) for handling tcp and [libuv](https://libuv.org/) to 
handle arbitrary stream buffers on 1 process.

Once you have those built, [compile the extension](https://www.sqlite.org/loadext.html). 
Using [`gcc`](https://gcc.gnu.org/) for example:

```bash
# omit include flags for brevity
gcc src/fetch.c src/stream.c -g -fPIC -shared -lcurl -luv
```

```bash
nvm use 22.4.1
CC=gcc CXX=g++ cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target fetch -j"$(nproc)"

```

And that's it!

You can also try the [`CMakeLists`](_media/CMakeLists.txt) if you have cmake.
The 4 commands to build the extension are:

```bash
mkdir build && cd build && cmake .. && make fetch
```

The default build flags link `libuv` and `libcurl` dynamically.

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

`fetch.c` uses the `__EMSCRIPTEN__` macro [emcc](https://emscripten.org/docs/tools_reference/emcc.html) plasters at compile-time to link the function implementations at compile-time:

```c
#ifdef __EMSCRIPTEN__ // WASM CASE with emcc
#include <emscripten.h>

// Some SQLite API functions take a pointer to a function that frees
// memory. Although we could add a C binding to a JavaScript function
// that calls sqlite3_free(), it is more efficient to pass the sqlite3_free
// function pointer directly. This function provides the C pointer to
// JavaScript.
void *EMSCRIPTEN_KEEPALIVE getSqliteFree() { return sqlite3_free; }

EM_ASYNC_JS(static response_t *, fetch,
            (const unsigned char *resource, const unsigned char *options), {
    const rmap = (globalThis.__rmap__ = globalThis.__rmap__ ?? {
        map: new Map(),
        count: 0
    });
...
```

So all the native code is in the `#else` branch of that check:

```c
#else // NATIVE CASE (with clang or gcc)
#include <curl/curl.h>
#include <uv.h>

/// This is roughly the equivalent of `await body.getReader().read()`
static size_t write_callback(void *contents, size_t size, size_t nmemb,
                             void *userp) {
    size_t total = size * nmemb;
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

For the full list, refer to the [`CMakeLists`](_media/CMakeLists.txt).

## npm release
This project has a [npm](npmjs.com) distribution to make integration into Javascript
apps easier. The main module is from [`src/index.ts`](_media/index.ts),
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
