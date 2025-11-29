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
and of course, SQLite.

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
