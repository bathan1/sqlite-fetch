insert into todos values (1, 1, 'hello sqlite fetch!', 0);
select id, "userId", title, completed from todos;
update todos set title = 'Hello SQLite Fetch' where id = 1;
delete from todos where id = 1;
