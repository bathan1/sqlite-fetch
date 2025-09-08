# Introduction
SQLite Fetch is a [runtime loadable extension](https://www.sqlite.org/loadext.html) that lets
you call [`fetch`](https://developer.mozilla.org/en-US/docs/Web/API/Fetch_API) inside SQLite. 
That's it.

<<< ../examples/sql/select_explicit.sql

If you are familiar with the `fetch` API, then you already know how to use SQLite Fetch.

## GET
`GET` calls are made using a SELECT query. You fetch web server Responses by the `url` column:

<<< ../examples/sql/select_where.sql

The Fetch virtual table is implemented as [eponymous-only](https://www.sqlite.org/vtab.html#eponymous_virtual_tables).
To read the `url` and the `headers` columns, you need to explicitly select them:

<<< ../examples/sql/select_all_and_hidden.sql

They need to be selected explicitly because they are declared as [HIDDEN](https://www.sqlite.org/vtab.html#hidden_columns_in_virtual_tables) columns.
So you can call them using the table valued function sugar as well:

<<< ../examples/sql/select_tvf.sql

## POST
`POST` calls are made using an INSERT query. Set the endpoint to `POST` to using the `url` column,
and the data to write using the `body` column:

<<< ../examples/sql/insert.sql

## PUT / PATCH
`PUT` and `PATCH` calls are made using an UPDATE query. By default, a `PUT` request is sent through.

<<< ../examples/sql/update.sql

Set the `type` column to 

## See Also
Here are some projects I found that were either direct inspirations
or just happened to have overlapping solutions...

1. [`sqlite-http`](https://github.com/asg017/sqlite-http):
This is probably the most similar project. `sqlite-http` provides more granular
functions to make HTTP requests and isn't based on Web APIs, while SQLite Fetch's `fetch` 
is the only way to interact with HTTP requests, and *is* roughly modeled after
the Web API interface.
