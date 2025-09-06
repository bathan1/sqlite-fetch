#!/usr/bin/env bash
set -euo pipefail

# Use your existing env (you already have EMSDK set)
EMSDK_ROOT="${EMSDK:-$HOME/emsdk}"
EMSCRIPTEN_ROOT="${EMSCRIPTEN:-$EMSDK_ROOT/upstream/emscripten}"
SYSROOT="${EMSCRIPTEN_SYSROOT:-$EMSCRIPTEN_ROOT/cache/sysroot}"

# Basic checks
[[ -d "$EMSCRIPTEN_ROOT" ]] || { echo "No EMSCRIPTEN at $EMSCRIPTEN_ROOT"; exit 1; }
[[ -d "$SYSROOT/include" ]] || { echo "No sysroot include at $SYSROOT/include (run emcc once)"; exit 1; }

# Write compile_flags.txt next to your sources
cat > compile_flags.txt <<EOF
-target
wasm32-unknown-emscripten
--sysroot=${SYSROOT}
-I${EMSCRIPTEN_ROOT}/system/include
-I${SYSROOT}/include
-I${SYSROOT}/include/emscripten
-D__EMSCRIPTEN__
-std=c11
EOF

echo "[ok] wrote $(pwd)/compile_flags.txt"
echo "[info] sysroot: $SYSROOT"
