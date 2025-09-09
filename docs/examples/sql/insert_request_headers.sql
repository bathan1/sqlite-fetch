insert into fetch (url, body, headers) values
    (
        'https://jsonplaceholder.typicode.com/posts',
        json_object(
            'userId', 1,
            'title', 'hello sqlite-fetch',
            'description', 'can i fetch that?'
        ),
        json_object('Content-Type', 'application/json')
    )
    returning *;
