.load ./libfetch.linux-x64
.mode box

create virtual table todos using fetch (
    'https://jsonplaceholder.typicode.com/todos',
    
    id int,
    title text
);

select id, title from todos;
