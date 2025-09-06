FROM node:22

# pnpm
RUN npm install -g pnpm

# Build deps for node-gyp and your CMake builds
RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 make g++ bash cmake curl git pkg-config \
    libssl-dev zlib1g-dev sqlite3 libsqlite3-dev \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app/sqlite-fetch
COPY . .

# Install node modules
RUN pnpm install --frozen-lockfile

# Build package -- no 'run' so pnpm can handle "postbuild" script
RUN pnpm build

CMD ["/bin/bash"]
