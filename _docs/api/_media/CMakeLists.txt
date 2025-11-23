cmake_minimum_required(VERSION 3.16)
project(fetch C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Let option() values we pass to subprojects override their defaults
if(POLICY CMP0077)
  cmake_policy(SET CMP0077 NEW)
endif()

# ---------------- Build toggles ----------------
option(BUILD_FETCH        "Build the network fetch extension"                ON)
option(BUILD_EXT          "Build other included SQLite extension(s)"         OFF)
option(THIRDPARTY_STATIC  "Build libuv/libcurl static and link into fetch"   OFF)

include(FetchContent)

# ---------------- Platform + arch naming ----------------
string(TOLOWER "${CMAKE_SYSTEM_NAME}" PLATFORM)
if(PLATFORM STREQUAL "darwin")
  set(PLATFORM "macos")
endif()
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
  set(ARCH "x64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
  set(ARCH "arm64")
else()
  set(ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif()

# ---------------- SQLite (headers-only; always fetched) ----------------
# You can override: -D SQLITE_VERSION=3380000 -D SQLITE_YEAR=2022
set(SQLITE_VERSION "3380000" CACHE STRING "sqlite src version")
set(SQLITE_YEAR    "2022"    CACHE STRING "sqlite release year (url path)")
set(SQLITE_URL "https://www.sqlite.org/${SQLITE_YEAR}/sqlite-src-${SQLITE_VERSION}.zip")

FetchContent_Declare(
  sqlite
  URL         ${SQLITE_URL}
  # URL_HASH  SHA256=<fill_me_for_reproducible_downloads>
  SOURCE_DIR  ${CMAKE_BINARY_DIR}/_deps/sqlite-src-${SQLITE_VERSION}
  UPDATE_DISCONNECTED TRUE
)
FetchContent_MakeAvailable(sqlite)
set(SQLITE_TREE "${sqlite_SOURCE_DIR}")

## yyjson for json parsing
FetchContent_Declare(
    yyjson
    GIT_REPOSITORY https://github.com/ibireme/yyjson.git
    GIT_TAG master # master, or version number, e.g. 0.6.0
)
FetchContent_GetProperties(yyjson)
if(NOT yyjson_POPULATED)
  FetchContent_Populate(yyjson)
  add_subdirectory(${yyjson_SOURCE_DIR} ${yyjson_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

# Interface target providing sqlite3.h/sqlite3ext.h
add_library(sqlite-headers INTERFACE)
target_include_directories(sqlite-headers INTERFACE "${SQLITE_TREE}")

# ---------------- Reusable SQLite extension helper ----------------
function(add_sqlite_ext EXT_NAME)
  set(options)
  set(oneValueArgs OUTPUT_NAME)
  set(multiValueArgs SOURCES LINK_LIBS)
  cmake_parse_arguments(ASE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT ASE_SOURCES)
    message(FATAL_ERROR "add_sqlite_ext(${EXT_NAME}): SOURCES is required")
  endif()

  set(ABS_SRCS "")
  foreach(rel IN LISTS ASE_SOURCES)
    set(src "${SQLITE_TREE}/${rel}")
    if(NOT EXISTS "${src}")
      message(FATAL_ERROR "add_sqlite_ext(${EXT_NAME}): source not found: ${src}")
    endif()
    list(APPEND ABS_SRCS "${src}")
  endforeach()

  add_library(${EXT_NAME} SHARED ${ABS_SRCS})
  set(out "lib${EXT_NAME}.${PLATFORM}-${ARCH}")
  if(ASE_OUTPUT_NAME)
    set(out "${ASE_OUTPUT_NAME}")
  endif()

  set_target_properties(${EXT_NAME} PROPERTIES
    PREFIX ""
    OUTPUT_NAME "${out}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    POSITION_INDEPENDENT_CODE ON
  )

  target_include_directories(${EXT_NAME} PRIVATE "${SQLITE_TREE}")
  target_compile_definitions(${EXT_NAME} PRIVATE SQLITE_THREADSAFE=2)

  # Do NOT link libsqlite3; resolve against host at load time
  if(APPLE)
    target_link_options(${EXT_NAME} PRIVATE "-Wl,-undefined,dynamic_lookup")
  elseif(UNIX)
    target_link_options(${EXT_NAME} PRIVATE "-Wl,--unresolved-symbols=ignore-in-shared-libs")
  endif()

  target_link_libraries(${EXT_NAME} PRIVATE sqlite-headers ${ASE_LINK_LIBS})
endfunction()

# ======================================================================
#                           FETCH EXTENSION
# ======================================================================
if(BUILD_FETCH)

  # ----------------------------------------------
  # libuv: system first (dynamic), else FetchContent
  # ----------------------------------------------
  unset(UV_TARGET)
  unset(UV_FROM_SYSTEM)
  if(NOT THIRDPARTY_STATIC)
    # Try system libuv
    find_path(LIBUV_INCLUDE_DIR NAMES uv.h)
    find_library(LIBUV_LIBRARY   NAMES uv libuv)
    if(LIBUV_INCLUDE_DIR AND LIBUV_LIBRARY)
      add_library(uv_system SHARED IMPORTED)
      set_target_properties(uv_system PROPERTIES
        IMPORTED_LOCATION "${LIBUV_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBUV_INCLUDE_DIR}"
      )
      set(UV_TARGET uv_system)
      set(UV_FROM_SYSTEM TRUE)
    endif()
  endif()

  # Fallback to FetchContent (dynamic or static) if no system uv or if THIRDPARTY_STATIC=ON
  if(NOT DEFINED UV_TARGET)
    if(THIRDPARTY_STATIC)
      set(UV_ARGS
        -DLIBUV_BUILD_SHARED=OFF
        -DLIBUV_BUILD_STATIC=ON
        -DBUILD_TESTING=OFF
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
      )
    else()
      set(UV_ARGS
        -DLIBUV_BUILD_SHARED=ON
        -DLIBUV_BUILD_STATIC=OFF
        -DBUILD_TESTING=OFF
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
      )
    endif()

    FetchContent_Declare(
      libuv
      GIT_REPOSITORY https://github.com/libuv/libuv.git
      GIT_TAG v1.49.2
      GIT_SHALLOW TRUE
      CMAKE_ARGS ${UV_ARGS}
    )
    FetchContent_MakeAvailable(libuv)

    if(THIRDPARTY_STATIC)
      set(UV_TARGET uv_a)
    else()
      set(UV_TARGET uv)
    endif()
  endif()

  # ----------------------------------------------
  # libcurl: system first (dynamic), else FetchContent
  # ----------------------------------------------
  unset(CURL_LINK_TARGET)
  unset(CURL_FROM_SYSTEM)
  if(NOT THIRDPARTY_STATIC)
    find_package(CURL QUIET)  # provides CURL::libcurl
    if(CURL_FOUND)
      set(CURL_LINK_TARGET CURL::libcurl)
      set(CURL_FROM_SYSTEM TRUE)
    endif()
  endif()

  if(NOT DEFINED CURL_LINK_TARGET)
    set(CURL_COMMON_ARGS
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON
      -DCURL_ZLIB=ON
      -DCMAKE_USE_OPENSSL=ON
      -DHTTP_ONLY=ON
      -DUSE_NGHTTP2=OFF
      -DCURL_BROTLI=OFF
      -DCURL_ZSTD=OFF
      -DENABLE_ARES=OFF
    )
    if(THIRDPARTY_STATIC)
      set(CURL_ARGS
        ${CURL_COMMON_ARGS}
        -DBUILD_SHARED_LIBS=OFF
        -DENABLE_SHARED=OFF
        -DENABLE_STATIC=ON
      )
    else()
      set(CURL_ARGS
        ${CURL_COMMON_ARGS}
        -DBUILD_SHARED_LIBS=ON
        -DENABLE_SHARED=ON
        -DENABLE_STATIC=OFF
      )
    endif()

    FetchContent_Declare(
      curl
      GIT_REPOSITORY https://github.com/curl/curl.git
      GIT_TAG curl-8_9_1
      GIT_SHALLOW TRUE
      CMAKE_ARGS ${CURL_ARGS}
    )
    FetchContent_MakeAvailable(curl)

    if(THIRDPARTY_STATIC)
      if(TARGET libcurl_static)
        set(CURL_LINK_TARGET libcurl_static)
      else()
        set(CURL_LINK_TARGET CURL::libcurl)
      endif()
    else()
      set(CURL_LINK_TARGET CURL::libcurl)
    endif()
  endif()

  # -------- Other deps (fetch-only) --------
  find_package(ZLIB REQUIRED)
  find_package(OpenSSL REQUIRED)

  # -------- fetch target (shared) --------
  add_library(fetch SHARED src/fetch.c)
  set_target_properties(fetch PROPERTIES
    PREFIX ""
    OUTPUT_NAME "libfetch.${PLATFORM}-${ARCH}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  )

  # If third-party are shared (either system or FC-shared), prefer $ORIGIN so colocated .so's are found
  if(NOT WIN32 AND NOT THIRDPARTY_STATIC)
    set_target_properties(fetch PROPERTIES
      BUILD_RPATH "\$ORIGIN"
      INSTALL_RPATH "\$ORIGIN"
    )
  endif()

  # Only add libuv build-tree includes when we are NOT using system uv
  if(NOT UV_FROM_SYSTEM AND TARGET uv)
    target_include_directories(fetch PRIVATE
      ${libuv_SOURCE_DIR}/include
      ${libuv_BINARY_DIR}/include
    )
  endif()

  if(UNIX AND NOT APPLE)
    set(EXTRA_SYS_LIBS pthread dl rt)
  endif()

  target_link_libraries(fetch PRIVATE
    yyjson
    ${CURL_LINK_TARGET}    # system or FC (static/dynamic)
    ${UV_TARGET}           # system uv, uv (shared) or uv_a (static)
    OpenSSL::SSL OpenSSL::Crypto
    ZLIB::ZLIB
    ${EXTRA_SYS_LIBS}
    sqlite-headers         # headers only; no linking to libsqlite3
  )

  if(THIRDPARTY_STATIC)
    target_compile_definitions(fetch PRIVATE CURL_STATICLIB)
  endif()

  # Allow unresolved sqlite3* to bind at load time (host SQLite)
  if(APPLE)
    target_link_options(fetch PRIVATE "-Wl,-undefined,dynamic_lookup")
  elseif(UNIX)
    target_link_options(fetch PRIVATE "-Wl,--unresolved-symbols=ignore-in-shared-libs")
  elseif(WIN32)
    # Do not link sqlite3.lib; extension resolves at runtime.
  endif()
endif() # BUILD_FETCH

# ======================================================================
#                         SQLITE EXTENSIONS (ext/)
# ======================================================================
if(BUILD_EXT)
  # CSV
  add_sqlite_ext(csv
    SOURCES ext/misc/csv.c
  )
  
  # UUID
  add_sqlite_ext(uuid
    SOURCES ext/misc/uuid.c
  )
endif() # BUILD_EXT
