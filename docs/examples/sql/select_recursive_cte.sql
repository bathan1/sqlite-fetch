/* This example shows how to page through FHIR Bundle links using a CTE.
   The JSON parsing is specific to FHIR, but the CTE pattern can work against
   just about any server that has an accessible next link in the Response data. */

.headers on
.load ./libfetch.linux-x64

create table pages (
    resource_type text,
    url text,
    data text,
    pgno int,

    -- Assuming just 1 FHIR server, this should be safe to do...
    primary key (resource_type, pgno)
);

with recursive patient_pages (url, data, depth) as (
    select
        'https://r4.smarthealthit.org/Patient',
        (select body from fetch('https://r4.smarthealthit.org/Patient')),
        0
    union all
    select
        links.value ->> 'url',
        (select fetch2.body from fetch(links.value ->> 'url') as fetch2),
        depth + 1
    from patient_pages, json_each(data -> 'link') as links
    where
        links.value ->> 'relation' = 'next'
        -- We have to manually set max depth here...
        and depth < 100
)
insert into pages select 'Patient', url, length(data) as data, depth + 1 as pgno from patient_pages;

-- output table
create table patients (
    id text,
    first_name text,
    last_name text,
    birth_date text
);

select * from pages;