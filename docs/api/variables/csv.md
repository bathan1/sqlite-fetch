[sqlite-fetch](../globals.md) / csv

# Variable: csv

> `const` **csv**: `string`

Defined in: [index.ts:73](https://github.com/nestorhealth/medfetch.js/blob/4acb837c034a09bf7dfa1acdf1604159b746493f/sqlite-fetch/src/index.ts#L73)

Best guess of [`csv`](https://sqlite.org/src/file/ext/misc/csv.c) extension path.

Assumes the binary is next to this file (index.js).

## Example

With [`better-sqlite3`](https://www.npmjs.com/package/better-sqlite3)
```js
import { csv } from "sqlite-fetch";
import Database from "better-sqlite3";

const db = new Database()
 .loadExtension(csv)
 .exec(`
     create virtual table "temp"."people" using
     csv(filename='people.csv', header=TRUE);
 `);

const people = db
 .prepare("select * from people")
 .all();
console.log("people from csv\n", people);
```
