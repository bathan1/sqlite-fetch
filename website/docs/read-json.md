---
sidebar_position: 2
---

# Read JSON

The Fetch virtual table was designed to work with JSONs by default.

*From* JSON *to* SQLite, the type conversions are:

|  JSON    |   SQLITE        |
|----------|-----------------|
| `string` | `TEXT`          |
| `object` | `TEXT`          |
| `array`  | `TEXT`          |
| `boolean`| `TEXT` or `INT` |
| `number` | `REAL` or `INT` |
| `null`   | `NULL`          |

## From `object` to Rows

Incoming data is the payload you receive from an HTTP `GET` request, which yarts
maps to a `SELECT` query against a Fetch virtual table.

```sql
SELECT "userId", id, title, completed FROM todos;
```

Here's what a todo from the [typicode api](https://jsonplaceholder.typicode.com/todos/1) looks like:

```json
{
  "userId": 1,
  "id": 1,
  "title": "delectus aut autem",
  "completed": false
}
```

To *read* a todo JSON as a SQL row, we have to tell the `fetch` virtual table
factory what SQL types we need converted from the JSON types.

## Easy Column Maps
The keys `userId` and `id` are both `number`s, and are represented as integers, so
we can map them to `INT`:

```sql
"userId" INT,
id INT
```

And `title` is a `string`, which we can just represent as `TEXT`:

```sql
"userId" INT,
id INT,
title TEXT
```

The Fetch virtual table works best with numerical / text types
because they have a logical 1-to-1 mapping between JSON and SQL.

## Booleans
While the keys so far have been easy to map, we have to make a decision with the 
`completed` field because SQLite doesn't support `BOOLEAN` column types.

Yarts lets you map JSON booleans to `TEXT` or `INT`.

If you choose to save `completed` columns as `TEXT`:

```sql title="todos_text.sql"
CREATE VIRTUAL TABLE todos USING fetch (
    url TEXT DEFAULT 'https://jsonplaceholder.typicode.com/todos',
    "userId" INT,
    id INT,
    title TEXT,
    completed TEXT
);
SELECT * FROM todos LIMIT 5;
```

Then yarts will map booleans to their text representation `true` and `false`:

```bash
sqlite> SELECT * FROM todos LIMIT 5;
┌────────┬────┬──────────────────────────────────────────────────────────────┬───────────┐
│ userId │ id │                            title                             │ completed │
├────────┼────┼──────────────────────────────────────────────────────────────┼───────────┤
│ 1      │ 1  │ delectus aut autem                                           │ false     │
├────────┼────┼──────────────────────────────────────────────────────────────┼───────────┤
│ 1      │ 2  │ quis ut nam facilis et officia qui                           │ false     │
├────────┼────┼──────────────────────────────────────────────────────────────┼───────────┤
│ 1      │ 3  │ fugiat veniam minus                                          │ false     │
├────────┼────┼──────────────────────────────────────────────────────────────┼───────────┤
│ 1      │ 4  │ et porro tempora                                             │ true      │
├────────┼────┼──────────────────────────────────────────────────────────────┼───────────┤
│ 1      │ 5  │ laboriosam mollitia et enim quasi adipisci quia provident il │ false     │
│        │    │ lum                                                          │           │
└────────┴────┴──────────────────────────────────────────────────────────────┴───────────┘
```

Boolean maps to `INT`s follow the standard C style bools where...

```sql title="todos_int.sql"
CREATE VIRTUAL TABLE todos USING fetch (
    url TEXT DEFAULT 'https://jsonplaceholder.typicode.com/todos',
    "userId" INT,
    id INT,
    title TEXT,
    completed INT
);
```

... `true` turns into `1` and `false` turns into `0`:

```bash
sqlite> SELECT * FROM todos LIMIT 5;
┌────────┬────┬──────────────────────────────────────────────────────────────┬───────────┐
│ userId │ id │                            title                             │ completed │
├────────┼────┼──────────────────────────────────────────────────────────────┼───────────┤
│ 1      │ 1  │ delectus aut autem                                           │ 0         │
├────────┼────┼──────────────────────────────────────────────────────────────┼───────────┤
│ 1      │ 2  │ quis ut nam facilis et officia qui                           │ 0         │
├────────┼────┼──────────────────────────────────────────────────────────────┼───────────┤
│ 1      │ 3  │ fugiat veniam minus                                          │ 0         │
├────────┼────┼──────────────────────────────────────────────────────────────┼───────────┤
│ 1      │ 4  │ et porro tempora                                             │ 1         │
├────────┼────┼──────────────────────────────────────────────────────────────┼───────────┤
│ 1      │ 5  │ laboriosam mollitia et enim quasi adipisci quia provident il │ 0         │
│        │    │ lum                                                          │           │
└────────┴────┴──────────────────────────────────────────────────────────────┴───────────┘
```

## Objects and Arrays
The last JSON types yarts has to handle are `object` and `array` types,
which it does by simply stringifying the values into `TEXT` columns.

A `user` JSON from the typicode API looks like this:

```json
{
  "id": 1,
  "name": "Leanne Graham",
  "username": "Bret",
  "email": "Sincere@april.biz",
  // highlight-start
  "address": {
    "street": "Kulas Light",
    "suite": "Apt. 556",
    "city": "Gwenborough",
    "zipcode": "92998-3874",
    "geo": {
      "lat": "-37.3159",
      "lng": "81.1496"
    }
  },
  // highlight-end
  "phone": "1-770-736-8031 x56442",
  "website": "hildegard.org",
  // highlight-start
  "company": {
    "name": "Romaguera-Crona",
    "catchPhrase": "Multi-layered client-server neural-net",
    "bs": "harness real-time e-markets"
  }
  // highlight-end
}
```

We can declare the `users` table like this, where we set the
`address` and `company` object values as `TEXT`s:

```sql
CREATE VIRTUAL TABLE users USING fetch (
    url TEXT DEFAULT 'https://jsonplaceholder.typicode.com/users',
    id INT,
    name TEXT,
    address TEXT,
    company TEXT
);
```

SQLite will just save it as plaintext:

```bash
sqlite> SELECT * FROM users LIMIT 2;
┌────┬───────────────┬──────────────────────────────┬────────────────────────────────────────────────────────────┐
│ id │     name      │           address            │                          company                           │
├────┼───────────────┼──────────────────────────────┼────────────────────────────────────────────────────────────┤
│ 1  │ Leanne Graham │ {                            │ {                                                          │
│    │               │   "street": "Kulas Light",   │   "name": "Romaguera-Crona",                               │
│    │               │   "suite": "Apt. 556",       │   "catchPhrase": "Multi-layered client-server neural-net", │
│    │               │   "city": "Gwenborough",     │   "bs": "harness real-time e-markets"                      │
│    │               │   "zipcode": "92998-3874",   │ }                                                          │
│    │               │   "geo": {                   │                                                            │
│    │               │     "lat": "-37.3159",       │                                                            │
│    │               │     "lng": "81.1496"         │                                                            │
│    │               │   }                          │                                                            │
│    │               │ }                            │                                                            │
├────┼───────────────┼──────────────────────────────┼────────────────────────────────────────────────────────────┤
│ 2  │ Ervin Howell  │ {                            │ {                                                          │
│    │               │   "street": "Victor Plains", │   "name": "Deckow-Crist",                                  │
│    │               │   "suite": "Suite 879",      │   "catchPhrase": "Proactive didactic contingency",         │
│    │               │   "city": "Wisokyburgh",     │   "bs": "synergize scalable supply-chains"                 │
│    │               │   "zipcode": "90566-7771",   │ }                                                          │
│    │               │   "geo": {                   │                                                            │
│    │               │     "lat": "-43.9509",       │                                                            │
│    │               │     "lng": "-34.4618"        │                                                            │
│    │               │   }                          │                                                            │
│    │               │ }                            │                                                            │
└────┴───────────────┴──────────────────────────────┴────────────────────────────────────────────────────────────┘
```

If you want to extract nested object values, use a `GENERATED ALWAYS AS` column
in conjunction with the `json_extract` SQLite function like so:

```sql
CREATE VIRTUAL TABLE users USING fetch (
    url TEXT DEFAULT 'https://jsonplaceholder.typicode.com/users',
    id INT,
    name TEXT,
    -- highlight-start
    address_street TEXT GENERATED ALWAYS AS (address->'street'),
    address_zipcode TEXT GENERATED ALWAYS AS (address->'zipcode'),
    address_geo_lat TEXT GENERATED ALWAYS AS (address->'geo'->'lat'),
    address_geo_lng TEXT GENERATED ALWAYS AS (address->'geo'->'lng')
    -- highlight-end
);
```

```bash
sqlite> SELECT * FROM users LIMIT 2;
┌────┬───────────────┬────────────────┬─────────────────┬─────────────────┬─────────────────┐
│ id │     name      │ address_street │ address_zipcode │ address_geo_lat │ address_geo_lng │
├────┼───────────────┼────────────────┼─────────────────┼─────────────────┼─────────────────┤
│ 1  │ Leanne Graham │ Kulas Light    │ 92998-3874      │ -37.3159        │ 81.1496         │
│ 2  │ Ervin Howell  │ Victor Plains  │ 90566-7771      │ -43.9509        │ -34.4618        │
└────┴───────────────┴────────────────┴─────────────────┴─────────────────┴─────────────────┘
```

If you have deeply nested values that are only separated by `object` parents,
then the `GENERATED ALWAYS AS` extraction is the best way to read that directly.

## Extract Nested Values
The Fetch virtual table assumes the endpoint at `url` returns a JSON array of objects.
If it returns a single object, then that object will be treated as the only row of the
table.

The FHIR API specs always return a single [Bundle](https://hl7.org/fhir/R4/bundle.html) object for 
its resources. So attempting to make this `patients` table.

```sql
CREATE VIRTUAL TABLE patients USING fetch (
    url TEXT DEFAULT 'https://r4.smarthealthit.org/Patient',
    "resourceType" TEXT,
    id TEXT
);
```

Will result in this result set:

```bash
sqlite> SELECT * FROM patients;
┌──────────────┬──────────────────────────────────────┐
│ resourceType │                  id                  │
├──────────────┼──────────────────────────────────────┤
│ Bundle       │ 326c2224-482a-4aff-aa09-9fdabfcc601c │
└──────────────┴──────────────────────────────────────┘
```

Of course, we'd like to be able to view the `Bundle.entry.resource` array field
as the rows.

We can do so by setting the `body` hidden column against `patients` to specify
the JSON path we want to follow from the body using [JSONPath](https://en.wikipedia.org/wiki/JSONPath):

```sql
SELECT * FROM patients
WHERE body = '$.entry[*].resource';
```

```bash
sqlite> SELECT * FROM patients WHERE body = '$.entry[*].resource';
TODO: PASTEME
```

So you have some flexibility when it comes to the exact shape of your API resources.
