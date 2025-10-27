# Introduction
SQLite Fetch is a [runtime loadable extension](https://www.sqlite.org/loadext.html) that allows
you to make [`Fetch API`](https://developer.mozilla.org/en-US/docs/Web/API/Fetch_API) calls directly from 
your SQLite.

## Installation
You can get the extension through the standalone [releases]() page,
or [build it from source](../api/index.md). Once you have that, link it to your SQLite client.
From the shell for example:

```sql
-- can omit the file extension (e.g. .so/.dylib/.dll)
.load ./libfetch.linux-x64
```

### Javascript
If you're using SQLite with JavaScript then you can install 
the extension through the npm release:

```bash
pnpm add sqlite-fetch
```

The exported [`Fetch`](../api/variables/Fetch.md) variable holds the path 
to the precompiled extension that matches your system. Then point your SQLite client to that path. Using [`better-sqlite3`](https://www.npmjs.com/package/better-sqlite3):

The ESM release ships some extra SQLite extension paths.
You just need to build them, which you can either do [manually](../api/index.md)
or with the convenience `sqlite-fetch` Node.js script in the npm release.

### [CSV](../api/variables/csv.md)
```bash
npx sqlite-fetch csv
```

### [UUID](../api/variables/uuid.md)
```bash
npx sqlite-fetch uuid
```
