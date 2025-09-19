create virtual table patients using fetch (
    url = 'https://r4.smarthealthit.org/Patient',
    body = '$.entry[*].resource',
    id text,
    meta text
);
