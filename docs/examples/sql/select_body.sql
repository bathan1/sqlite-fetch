.headers on
.mode box
.load ./libfetch.linux-x64

select 
    b.value ->> 'userId' as "userId",
    b.value ->> 'id' as "id",
    b.value ->> 'title' as "title",
    b.value ->> 'completed' as "completed"
from fetch('https://jsonplaceholder.typicode.com/todos'), json_each(body) as b limit 1;

create table todos as select 
    b.value ->> 'userId' as "userId",
    b.value ->> 'id' as "id",
    b.value ->> 'title' as "title",
    b.value ->> 'completed' as "completed"
from fetch('https://jsonplaceholder.typicode.com/todos'), json_each(body) as b;

select * from todos limit 10;