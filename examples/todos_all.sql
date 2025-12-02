-- This fetches all todos from the dummy typicode endpoint.
-- Columns need to match top level container's element object keys.
CREATE VIRTUAL TABLE todos USING fetch (
    url TEXT DEFAULT 'http://jsonplaceholder.typicode.com/todos',
    /*
        In typescript pseudocode, a todo looks like:
        `type Todo = { userId: number; id: number; title: string; completed: boolean; }`
    */
    "userId" INT,
    id INT,
    title TEXT,
    completed TEXT

    -- This will be converted from JSON value (true / false) into
    -- text bytes "true" or "false".
);

SELECT * FROM todos LIMIT 1;
