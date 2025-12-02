DROP TABLE IF EXISTS todos;
CREATE VIRTUAL TABLE todos USING fetch (
    url TEXT DEFAULT 'https://jsonplaceholder.typicode.com/todos',
    id INT,
    "userId" INT,
    title TEXT,
    completed INT
);
SELECT * FROM todos LIMIT 5;
