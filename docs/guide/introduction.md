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
`POST` calls are made using an INSERT query. Set the endpoint with the `url` column,
and the data to write using the `body` column:

<<< ../examples/sql/insert.sql

## PUT / PATCH
`PUT` and `PATCH` calls are made using an UPDATE query. By default, a `PUT` request is sent through.

<<< ../examples/sql/update.sql

Set the `type` column to 'patch' if you want to make it a `PATCH` request. Casing doesn't matter,
just as long as it spells out `patch`:


<<< ../examples/sql/update_patch.sql

## DELETE
`DELETE` calls are made using a DELETE query:

<<< ../examples/sql/delete.sql

## Next Steps
See the [installation](./getting-started.md) page to download the extension. Or go straight to [the examples]()
section to see how SQLite Fetch can be used.
