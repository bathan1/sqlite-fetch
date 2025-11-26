-- copy users as-is from into sqlite memory
CREATE VIRTUAL TABLE users USING fetch (
    url TEXT DEFAULT 'http://jsonplaceholder.typicode.com/users',
    id INT,
    name TEXT,
    username TEXT,
    email TEXT,
    address TEXT,
    phone TEXT,
    website TEXT,
    company TEXT
);

SELECT * FROM users;
