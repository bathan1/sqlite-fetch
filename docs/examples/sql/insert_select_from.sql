create table patients (
    id text,
    first_name text,
    last_name text,
    birth_date text
);

insert into patients select
    e.value -> 'resource' ->> 'id',
    e.value -> 'resource' -> 'name' -> 0 -> 'given' ->> 0,
    e.value -> 'resource' -> 'name' -> 0 ->> 'family',
    e.value -> 'resource' ->> 'birthDate'
from fetch('https://r4.smarthealthit.org/Patient'), json_each(body -> 'entry') as e;

create table conditions (
    id text primary key,
    reference text not null references patients (id)
);

insert into conditions select
    e.value -> 'resource' ->> 'id' as id,
    substr(e.value -> 'resource' -> 'subject' ->> 'reference', 9) as reference
from fetch('https://r4.smarthealthit.org/Condition'), json_each(body -> 'entry') as e;

select * from conditions inner join patients on patients.id = conditions.reference;