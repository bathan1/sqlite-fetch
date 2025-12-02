DROP TABLE IF EXISTS todos;
CREATE VIRTUAL TABLE todos USING fetch (
    url TEXT DEFAULT 'https://jsonplaceholder.typicode.com/todos',
    "userId" INT,
    id INT,
    title TEXT,
    completed INT
);
SELECT * FROM todos LIMIT 5;
