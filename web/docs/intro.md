---
sidebar_position: 1
---

# Introduction

Let's discover **SQLite Fetch in less than 5 minutes**.

## Getting Started

Get started by **installing the latest extension binary**.

Or **try SQLite Fetch immediately on Web Assembly** [here]().

### What you'll need

- [SQLite](https://sqlite.org/) version 3.45 or higher.

## Link the Extension

Install the extension from the Github releases page and open SQLite. 
The default release is Linux x86_64:

```bash
curl -LO https://github.com/bathan1/sqlite-fetch/releases/download/latest/libfetch.so
sqlite3
```

Load the extension:

```sql
.load ./libfetch
```

This links the Fetch virtual table library with SQLite.

## Write your Queries

Create a Virtual Table:

```sql
create virtual table todos using fetch('https://jsonplaceholder.typicode.com/todos');
```

The first argument of `fetch` is the url of the API you want to query. Everything afterwards 
are SQLite column declarations that should match the data the server returns.


To query all rows from it:

```sql
select * from todos;
```

To query all completed todos:

```sql
select * from todos where completed = 'true';
```

The dummy todos api returns a JSON array. And by default, 
`fetch` parses the response body as a JSON array of objects,
and uses the object's keys as the columns.

If you only cared about the `id` and `title` keys,
you can tell `fetch` to store only those keys as the columns:

```sql
drop table if exists todos;
create virtual table todos using fetch (
    'https://jsonplaceholder.typicode.com/todos',
    id int,
    title text
);
select * from todos;
```

If the server returns an object, then SQLite Fetch will treat that as the only row by default:
```sql
create virtual table patients using fetch(
    'https://r4.smarthealthit.org/Patient'
);
select * from patients;
```

You can point the virtual table to a nested path by setting the `body` argument to a [jsonpath]():
```sql
drop table if exists patients;
create virtual table patients using fetch (
    'https://r4.smarthealthit.org/Patient',
    body='$.entry[*].resource'
);
select * from patients;
```

While JSON is the default, you can parse the payload into [other formats]()  as well.
