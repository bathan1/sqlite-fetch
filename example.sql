.mode box
.load ./libyarts

create virtual table users using fetch (
    url text default 'http://jsonplaceholder.typicode.com/users',
    id int,
    name text
);

select * from users;
