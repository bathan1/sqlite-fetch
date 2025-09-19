select ok from fetch where url = 'https://jsonplaceholder.typicode.com/todos';

-- argv[0] = "main"
-- argv[1] = "fetch"
-- argv[2] = "todos"
-- argv[3] = "url = '...'"
-- argv[4] = "id int"
create virtual table todos using fetch (
    url = 'https://jsonplaceholder.typicode.com/todos',
    id int,
    "userId" int,
    title text,
    completed int
);

select * from todos;
