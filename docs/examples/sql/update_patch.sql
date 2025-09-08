-- Manually set method to PATCH
update fetch set
    body = json_object(
        'userId', 1,
        'title', 'Hello SQLite Fetch',
        'description', 'Can I fetch that?'
    )
where url = 'https://jsonplaceholder.typicode.com/posts/1' and type = 'PATCH'
