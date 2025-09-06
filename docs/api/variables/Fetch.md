[sqlite-fetch](../globals.md) / Fetch

# Variable: Fetch

> `const` **Fetch**: `string`

Defined in: [index.ts:48](https://github.com/nestorhealth/medfetch.js/blob/4acb837c034a09bf7dfa1acdf1604159b746493f/sqlite-fetch/src/index.ts#L48)

Best guess of `Fetch` extension path. The extension is precompiled in
the esm release, so you don't need to build it.

Assumes the binary is next to this file (`index.js`).

## Example

With [`better-sqlite3`](https://www.npmjs.com/package/better-sqlite3)
```js
import { Fetch } from "sqlite-fetch";
import Database from "better-sqlite3";

const db = new Database().loadExtension(Fetch)
const todos = db
.prepare(`select ok, status from fetch(?);`)
.get("https://jsonplaceholder.typicode.com/todos");
console.log("todos from fetch\n", todos);
```
