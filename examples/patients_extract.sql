CREATE VIRTUAL TABLE patients USING fetch (
    url TEXT DEFAULT 'https://r4.smarthealthit.org/Patient',
    id TEXT,
    meta TEXT GENERATED ALWAYS AS ('entry'->'*'->'resource'->'meta'),
    name TEXT GENERATED ALWAYS AS ('entry'->'*'->'resource'->'name')
);

SELECT * FROM patients;
