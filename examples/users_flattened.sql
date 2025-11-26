-- Traverses user 'address' and 'company' object JSON keys
-- to extract deeper scalar values.
CREATE VIRTUAL TABLE users USING fetch (
    url TEXT DEFAULT 'http://jsonplaceholder.typicode.com/users',
    id INT,
    name TEXT,
    username TEXT,
    email TEXT,

    -- begin flat address
    address_street TEXT GENERATED ALWAYS AS (address->'street'),
    address_suite TEXT GENERATED ALWAYS AS (address->'suite'),
    address_city TEXT GENERATED ALWAYS AS (address->'city'),
    address_zipcode TEXT GENERATED ALWAYS AS (address->'zipcode'),
    -- begin flat address.geo
    address_geo_lat TEXT GENERATED ALWAYS AS (address->'geo'->'lat'),
    address_geo_lng TEXT GENERATED ALWAYS AS (address->'geo'->'lng'),
    -- end flat address.geo
    -- end flat address

    phone TEXT,
    website TEXT,

    -- begin flat company
    company_name TEXT GENERATED ALWAYS AS (company->'name'),
    company_catch_phrase TEXT GENERATED ALWAYS AS (company->'catchPhrase'),
    company_bs TEXT GENERATED ALWAYS AS (company->'bs')
    -- end flat company
);

SELECT * FROM users;
