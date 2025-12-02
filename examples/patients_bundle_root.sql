-- This selects the Patient Bundle parent
CREATE VIRTUAL TABLE patients USING fetch (
    url TEXT DEFAULT 'https://r4.smarthealthit.org/Patient',
    id TEXT,
    "resourceType" TEXT,
    meta TEXT,
    name TEXT
);

SELECT * FROM patients;
