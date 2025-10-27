# API Design
The `Fetch` virtual table maps SQL queries to [HTTP Requests](https://developer.mozilla.org/en-US/docs/Web/HTTP/Guides/Messages#http_requests).
| SQL Query | HTTP Method |
|-----------|-------------|
| `SELECT`    | GET         |
| `INSERT`    | POST        |
| `UPDATE`    | PUT         |
| `DELETE`    | DELETE |

## SELECT
`SELECT` maps to `GET` requests.
::: code-group
```sql
select * from fetch
where url = 'https://jsonplaceholder.typicode.com/todos';
```
```bash
curl https://jsonplaceholder.typicode.com/todos
```
:::

## INSERT
`INSERT` maps to `POST` requests. Specifying the url via the `url` column is required.
::: code-group
```sql
insert into fetch (url, body) values
('https://jsonplaceholder.typicode.com/todos', json_object(
    'userId', 1,
    'title', 'hello SQLite Fetch!',
));
```
```bash
curl -X POST https://jsonplaceholder.typicode.com/todos \
    -H "Content-Type: application/json" \
    -d '{
        "userId": 1,
        "title": "hello SQLite Fetch!"
    }'
```

## UPDATE
`UPDATE` maps to `PUT` requests *by default*. Specifying the url via the `url` column
**in the `WHERE` clause** is required:
::: code-group
```sql
update fetch
set body = json_object(
    'userId', 1,
    'title', 'Hello SQLite Fetch' -- fix capitalization
)
where url = 'https://jsonplaceholder.typicode.com/todos/101';
```
```bash
curl -X PUT https://jsonplaceholder.typicode.com/todos/101 \
    -H "Content-Type: application/json" \
    -d '{
        "userId": 1,
        "title": "Hello SQLite Fetch"
    }'
```
:::

The `type` column can be set to the text literal `'PATCH'` (casing doesn't matter) to change
the method in the request line to `'PATCH'`:
::: code-group
```sql
update fetch
set body = json_object('title', 'Hello SQLite Fetch') -- "userId" prop stays the same
where url = 'https://jsonplaceholder.typicode.com/todos/101'
and type = 'PATCH';
```
```bash
curl -X PATCH https://jsonplaceholder.typicode.com/todos/101 \
    -H "Content-Type: application/json" \
    -d '{
        "title": "Hello SQLite Fetch"
    }'
```

## DELETE
`DELETE` maps to a `DELETE` request line. Specifying the 'url' via the `url` column in the `WHERE`
clause is required.
::: code-group
```sql
delete from fetch where url = 'https://jsonplaceholder.typicode.com/101';
```
```bash
curl -X DELETE https://jsonplaceholder.typicode.com/todos/101
```
:::
