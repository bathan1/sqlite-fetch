# Getting Started
Targets are based on the [Node.js os](https://nodejs.org/api/os.html) API.
You can get the extension through the standalone [releases]() page,
or [build it from source](../api/index.md).
Once you have that, you can link it to your SQLite client. On the shell:

```sql
-- can omit the file extension (e.g. .so/.dylib/.dll)
.load ./libfetch.linux-x64

select body from fetch('https://jsonplaceholder.typicode.com/todos');
```

## Javascript
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
