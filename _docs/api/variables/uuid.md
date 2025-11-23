[sqlite-fetch](../globals.md) / uuid

# Variable: uuid

> `const` **uuid**: `string`

Defined in: [index.ts:90](https://github.com/nestorhealth/medfetch.js/blob/4acb837c034a09bf7dfa1acdf1604159b746493f/sqlite-fetch/src/index.ts#L90)

Best guess of [`uuid`](https://sqlite.org/src/file/ext/misc/uuid.c) extension path.

Assumes the binary is next to this file (index.js).

## Example

With [`better-sqlite3`](https://www.npmjs.com/package/better-sqlite3)
```ts
import { uuid } from "sqlite-fetch";
import Database from "better-sqlite3";

const db = new Database().loadExtension(uuid);
const uuidValue = db.prepare(`select uuid();`).get();
console.log("uuid from uuid\n", uuidValue);
```
