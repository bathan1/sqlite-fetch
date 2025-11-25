---
sidebar_position: 1
---

# Introduction

Yet Another Runtime TCP Stream for SQLite, or yarts for short,
is an SQLite runtime extension that provides TCP networking capabilities to SQLite.
Let's discover **yarts in less than 5 minutes**.

## Getting Started

Get started by **installing the latest extension binary**.

Or **try yarts immediately on the Web Assembly build** [here]().

### What you'll need

- [SQLite](https://sqlite.org/) version 3.45 or higher.

## Link the Extension

Install the extension from the Github releases page and open SQLite. 
The default release is Linux x86_64:

```bash
curl -LO https://github.com/bathan1/sqlite-fetch/releases/download/latest/libyarts.so
sqlite3
```

Load the extension:

```sql
.load ./libyarts
```

Optionally format the cli:

```sql
.mode box
```

This links the `fetch` virtual table library with SQLite.

## Write your Queries

Create a Virtual Table by declaring your expected payload shape with the `fetch` virtual table:

```sql
CREATE VIRTUAL TABLE todos USING fetch (
    "userId" INT,
    id INT,
    title TEXT,
    completed TEXT
);
```

Fetch will include a `url HIDDEN TEXT` column into 
your virtual table, which will provide the url to fetch from.

To fetch some `todos` from a json dummy api, for example,
we set `url` equal to the endpoint in `SELECT ... WHERE ...` query:

```sql
SELECT * FROM todos
WHERE url = 'https://jsonplaceholder.typicode.com/todos' LIMIT 5;
```

To query all completed todos:

```sql
SELECT * FROM todos WHERE 
url = 'https://jsonplaceholder.typicode.com/todos'
AND completed = 'true' LIMIT 5;
```

If you only cared about the `id` and `title` fields, you
can simply omit the other fields:

```sql
DROP TABLE IF EXISTS todos;
CREATE VIRTUAL TABLE todos USING fetch (
    id INT,
    title TEXT
);
SELECT * FROM todos
WHERE url = 'https://jsonplaceholder.typicode.com/todos'
LIMIT 5;
```

Since `url` is a hidden column, you can query the url column
in a table valued function sugar syntax, which is equivalent
to the above:

```sql
SELECT * FROM todos('https://jsonplaceholder.typicode.com/todos')
LIMIT 5;
```

If you're only fetching from one server, you can set a default value 
for the `url` column in the create virtual table statement:

```sql
DROP TABLE IF EXISTS todos;
CREATE VIRTUAL TABLE todos USING fetch (
    url TEXT DEFAULT 'https://jsonplaceholder.typicode.com/todos',
    id INT,
    title TEXT
);
```

This way, you don't need to set the `url` column in each
query:

```sql
SELECT * FROM todos LIMIT 5;
```

