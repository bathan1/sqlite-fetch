select ok from fetch where url = 'https://jsonplaceholder.typicode.com/users';
select ok from fetch where url = 'https://jsonplaceholder.typicode.com/todos';

create virtual table users2 using fetch;

create virtual table users using fetch (
    id int,
    name text,
    username text,
    address text,
    phone text,
    website text,
    company text
);

create virtual table todos using fetch (
    id int,
    "userId" int,
    title text,
    completed int
);
