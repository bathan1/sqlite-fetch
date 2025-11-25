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

This links the `fetch` virtual table library with SQLite.

## Write your Queries

Create a Virtual Table by declaring your expected payload shape with the `fetch` virtual table:

```sql
create virtual table todos using fetch (
    "userId" int,
    id int,
    title text,
    completed text
);
```

The virtual table will include a `url hidden text` column into 
your virtual table, which will provide the url to fetch from.
To fetch some `todos` from a json dummy api, for example:

```sql
select * from todos
where url = 'https://jsonplaceholder.typicode.com/todos';
```

To query all completed todos:

```sql
select * from todos where 
url = 'https://jsonplaceholder.typicode.com/todos';
completed = 'true';
```

If you only cared about the `id` and `title` fields, you
can simply omit the other fields:

```sql
drop table if exists todos;
create virtual table todos using fetch (
    id int,
    title text
);
select * from todos
where url = 'https://jsonplaceholder.typicode.com/todos';
```

If you're only fetching from one server, you can set a default value for the `url` column 
in the create virtual table statement:

```sql
create virtual table todos using fetch (
    url text default 'https://jsonplaceholder.typicode.com/todos',
    id int,
    title text
);

select * from todos;
```
