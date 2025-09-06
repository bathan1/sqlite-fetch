<script lang="ts" setup>
import { onMounted, ref } from "vue";
import DataTable from "@/components/DataTable.vue";
import Database from "../../src/sqlite-wasm/Database.js"

const sql = 
`WITH RECURSIVE paged (url, payload, depth) AS (
    -- Base case
    SELECT
        'https://r4.smarthealthit.org/Patient',
        (SELECT json FROM fetch('https://r4.smarthealthit.org/Patient')),
        0

    UNION ALL

    -- Recursive step
    SELECT
        links.value ->> 'url',
        (SELECT fetch2.json FROM fetch(links.value ->> 'url') as fetch2),
        depth + 1
    FROM paged,
         json_each(paged.payload -> 'link') AS links
    WHERE
        links.value ->> 'relation' = 'next'
        AND depth < 1
)

-- Flatten all entries across all payloads
SELECT 
    entries.value -> 'resource' ->> 'id' as "id",
    (entries.value -> 'resource' -> 'name' -> 0 -> 'given' ->> 0) 
    || ' '
    || (entries.value -> 'resource' -> 'name' -> 0 ->> 'family') as "name",
    entries.value -> 'resource' ->> 'birthDate' as "birthDate"
FROM 
    paged,
    json_each(paged.payload -> 'entry') as entries;`;

const columns = ref<{name: string; dataType?: string;}[]>([]);
const rows = ref<Record<string, unknown>[]>([]);
onMounted(async () => {
    const db = new Database();
    const result = await db.query(true, sql);
    console.log({ result })
    if (result.rows.length > 0) {
        // Auto-detect columns from first row
        columns.value = Object.keys(result.rows[0]).map((k) => ({ name: k }));
        rows.value = result.rows;
    }
})
</script>

# Paging
Many APIs have some mechanism of pagination so they don't have
to return all the data in one go to squeeze out as much
**throughput** on its responses. 

As of the time of writing, there is no standard on how to
encode pagination on a Response, so handling pagination
is left up to the caller (you).

But if you know where the 'next page' data resides in the Response
body, then paging can be achieved through a [recursive CTE](https://sqlite.org/lang_with.html):

```sql
WITH RECURSIVE paged (url, payload, depth) AS (
    -- Base case
    SELECT
        'https://r4.smarthealthit.org/Patient',
        (SELECT json FROM fetch('https://r4.smarthealthit.org/Patient')),
        0

    UNION ALL

    -- Recursive step
    SELECT
        links.value ->> 'url',
        (SELECT fetch2.json FROM fetch(links.value ->> 'url') as fetch2),
        depth + 1
    FROM paged,
         json_each(paged.payload -> 'link') AS links
    WHERE
        links.value ->> 'relation' = 'next'
        AND depth < 1
)

-- Flatten all entries across all payloads
SELECT 
    entries.value -> 'resource' ->> 'id' as "id",
    (entries.value -> 'resource' -> 'name' -> 0 -> 'given' ->> 0) 
    || ' '
    || (entries.value -> 'resource' -> 'name' -> 0 ->> 'family') as "name",
    entries.value -> 'resource' ->> 'birthDate' as "birthDate"
FROM 
    paged,
    json_each(paged.payload -> 'entry') as entries;
```

<DataTable v-if="rows.length > 0" :columns="columns" :rows="rows" />