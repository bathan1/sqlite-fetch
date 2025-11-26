-- This selects Patient resources from a flat Bundle
CREATE VIRTUAL TABLE patients USING fetch (
    url TEXT DEFAULT 'http://r4.smarthealthit.org/patient',
    id TEXT,
    meta TEXT GENERATED ALWAYS AS ('entry'->'*'->'resource'->'meta'),
    name TEXT GENERATED ALWAYS AS ('entry'->'*'->'resource'->'name')
);

/* */

SELECT * FROM patients;
