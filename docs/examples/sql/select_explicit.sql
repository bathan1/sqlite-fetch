select
    body,
    body_used,
    headers,
    ok,
    redirected,
    status,
    status_text,
    type,
    url
from fetch('https://sqlite-fetch.dev');