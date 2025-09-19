select ok from fetch where url = 'https://jsonplaceholder.typicode.com/todos';

create virtual table todos using fetch (
    url = 'https://jsonplaceholder.typicode.com/todos',
    id int,
    "userId" int,
    title text,
    completed int
);

select * from todos;
