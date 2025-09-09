select headers from fetch where url = 'https://jsonplaceholder.typicode.com/todos' and headers = json_object(
    'Content-Type', 'application/json'
);
