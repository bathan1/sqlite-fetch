.mode box
.load ./libyarts

create virtual table users using fetch (
    url text default 'http://jsonplaceholder.typicode.com/users',
    id int,
    name text,
    username text,
    email text,
    address_street text generated always as (address->'street'),
    phone text,
    website text
);

select * from users;
