# yarts
Yet Another Runtime TCP Stream (for SQLite).

| OS      | Arch  | Tested |
|---------|-------|--------|
| Linux   | x64   | ✅      |
| Linux   | arm64 | ❌      |
| macOS   | x64   | ❌      |
| macOS   | arm64 | ❌      |
| Windows | x64   | ❌      |
| Windows | arm64 | ❌      |

## Building
To build using the Makefile, you need the following libraries installed onto your *system* lib:
    - [yajl](https://github.com/lloyd/yajl) for stream parsing.
    - [yyjson](https://github.com/ibireme/yyjson) to be able to work with JSONs in C without going insane.
    - [libcurl](https://curl.se/libcurl/) to parse URLs.
    - SQLite (duh!)

To install yajl and libcurl Ubuntu, for example:

```bash
sudo apt install libcurl4-openssl-dev
sudo apt install libyajl-dev
```

yyjson isn't available on apt, so we have to build from source:

```bash
git clone https://github.com/ibireme/yyjson
cd yyjson
mkdir build && cd build && cmake ..
sudo make install
```

Then `cd` back into the root and run:

```bash
make
```

And that's it!

## Other Scripts
To install / uninstall the yarts headers from your usr local lib:

```bash
sudo make install
sudo make uninstall
```

To run the test script, make sure you have the binary built at the project root.
Then you can run the npm script:

```bash
pnpm run test
```

## Library
The primary file is [yarts.h](./src/yarts.h), which defines the
SQLite loader function `sqlite3_yarts_init` **and** the networking
functions the extension uses itself.

The majority of the code is under the `helper.*.c` files, which is only
exposed to the [yarts.c](./src/yarts.c) file so that it doesn't pollute
the yarts "public" API.

## Documentation
This project uses [Doxygen](https://www.doxygen.nl/) for code documentation
and uses [Docusaurus](http://docusaurus.io/) for the tutorial docs.
