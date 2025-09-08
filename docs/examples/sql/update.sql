-- default UPDATE uses method PUT
update fetch set
    body = json_object(
        'userId', 1,
        'title', 'Hello SQLite Fetch',
        'description', 'Can I fetch that?'
    )
where url = 'https://jsonplaceholder.typicode.com/posts/1';
