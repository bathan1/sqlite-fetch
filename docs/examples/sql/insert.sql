.headers on
.mode box
.load ./libfetch.linux-x64

-- http post
insert into fetch (url, body) values
    (
        'https://jsonplaceholder.typicode.com/todos',
        json_object(
            'userId', 1,
            'title', 'hello sqlite-fetch',
            'description', 'can i fetch that?'
        )
    );