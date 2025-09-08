insert into fetch (url, body) values
    (
        'https://jsonplaceholder.typicode.com/posts',
        json_object(
            'userId', 1,
            'title', 'hello sqlite-fetch',
            'description', 'can i fetch that?'
        )
    )
    returning *;
