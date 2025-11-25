---
sidebar_position: 2
---

# Write JSON

*From* SQLite *to* JSON, the types are converted as:

|  SQLite  |   JSON                 |
|----------|------------------------|
| `BLOB`   | `string`               |
| `TEXT`   | `string` or `boolean`  |
| `INT`    | `number` or `boolean`  |
| `REAL`   | `number`               |
| `NULL`   | `null`                 |

