create virtual table todos using fetch (
    url = 'https://jsonplaceholder.typicode.com/todos',
    body = '',
    id int primary key,
    "userId" int,
    title text,
    completed int
);
