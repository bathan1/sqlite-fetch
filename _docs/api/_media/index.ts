import { fileURLToPath } from "node:url";
import { join } from "node:path";
import os from "node:os";

const platform = os.platform();
const arch = process.arch;

const computePath = (libName: string) => `lib${libName}.${platform}-${arch}`;

const EXTRA_EXTENSIONS = [
    "csv",
    "uuid"
];

const EXTENSIONS = [
    "fetch",
    ...EXTRA_EXTENSIONS
];

const [
    FETCH,
    CSV,
    UUID
] = EXTENSIONS.map(computePath);

const __dirname = fileURLToPath(
    new URL(import.meta.env.DEV ? "." : ".", import.meta.url),
);

/**
 * Best guess of `Fetch` extension path. The extension is precompiled in
 * the esm release, so you don't need to build it.
 *
 * Assumes the binary is next to this file (`index.js`).
 * @example
 * With [`better-sqlite3`](https://www.npmjs.com/package/better-sqlite3)
 * ```js
 * import { Fetch } from "sqlite-fetch";
 * import Database from "better-sqlite3";
 *
 * const db = new Database().loadExtension(Fetch)
 * const todos = db
 * .prepare(`select ok, status from fetch(?);`)
 * .get("https://jsonplaceholder.typicode.com/todos");
 * console.log("todos from fetch\n", todos);
 * ```
 */
export const Fetch = join(__dirname, FETCH);

/**
 * Best guess of [`csv`](https://sqlite.org/src/file/ext/misc/csv.c) extension path.
 *
 * Assumes the binary is next to this file (index.js).
 * @example
 * With [`better-sqlite3`](https://www.npmjs.com/package/better-sqlite3)
 * ```js
 * import { csv } from "sqlite-fetch";
 * import Database from "better-sqlite3";
 *
 * const db = new Database()
 *  .loadExtension(csv)
 *  .exec(`
 *      create virtual table "temp"."people" using
 *      csv(filename='people.csv', header=TRUE);
 *  `);
 *
 * const people = db
 *  .prepare("select * from people")
 *  .all();
 * console.log("people from csv\n", people);
 * ```
 */
export const csv = join(__dirname, CSV);

/**
 * Best guess of [`uuid`](https://sqlite.org/src/file/ext/misc/uuid.c) extension path.
 *
 * Assumes the binary is next to this file (index.js).
 * @example
 * With [`better-sqlite3`](https://www.npmjs.com/package/better-sqlite3)
 * ```ts
 * import { uuid } from "sqlite-fetch";
 * import Database from "better-sqlite3";
 *
 * const db = new Database().loadExtension(uuid);
 * const uuidValue = db.prepare(`select uuid();`).get();
 * console.log("uuid from uuid\n", uuidValue);
 * ```
 */
export const uuid = join(__dirname, UUID);
