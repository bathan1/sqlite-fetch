select ok from fetch where url = 'https://jsonplaceholder.typicode.com/users';
select ok from fetch where url = 'https://jsonplaceholder.typicode.com/todos';

create virtual table users using fetch (
    url = 'https://jsonplaceholder.typicode.com/users',
    id int,
    name text,
    username text,
    address text,
    phone text,
    website text,
    company text
);

create virtual table todos using fetch (
    url = 'https://jsonplaceholder.typicode.com/todos',
    id int,
    "userId" int,
    title text,
    completed int
);

select ok from fetch where url = 'https://jsonplaceholder.typicode.com/users';

select * from users;
select * from todos;
