<template>
  <div class="repl">
    <!-- #region attach -->
    <textarea
      v-model="sql"
      class="query"
      placeholder="Write SQL here..."
    ></textarea>
    <button @click="handleQuery">Run</button>
    <!-- #endregion attach -->


    <!-- #region render -->
    <div class="output" v-if="rows.length">
      <table>
        <thead>
          <tr>
            <th v-for="(col, i) in columns" :key="i">{{ col }}</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="(row, r) in rows" :key="r">
            <td v-for="col in columns" :key="col">{{ row[col] }}</td>
          </tr>
        </tbody>
      </table>
    </div>
    <!-- #endregion render -->

    <div v-else-if="message">
      <pre>{{ message }}</pre>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue'
import Database from "../../src/sqlite-wasm/Database";

const sql = ref(`
    SELECT
      entries.value -> 'resource' ->> 'id' as id,
      entries.value -> 'resource' ->> 'gender' as gender,
      entries.value -> 'resource' ->> 'birthDate' as "birthDate",
      entries.value -> 'resource' ->> 'name' as "name",
      entries.value -> 'resource' ->> 'active' as "active",
      entries.value -> 'resource' ->> 'maritalStatus' as "marialStatus", 
      entries.value -> 'resource' ->> 'multipleBirthBoolean' as "multipleBirthBoolean",
      entries.value -> 'resource' ->> 'multipleBirthInteger' as "multipleBirthInteger",
      entries.value -> 'resource' ->> 'deceasedDateTime' as "deceasedDateTime",
      entries.value -> 'resource' ->> 'address' as "address",
      entries.value -> 'resource' ->> 'generalPractitioner' as "generalPractitioner",
      entries.value -> 'resource' ->> 'communication' as "communication",
      entries.value -> 'resource' ->> 'managingOrganization' as "managingOrganization",
      entries.value -> 'resource' ->> 'photo' as "photo"
    FROM fetch('https://r4.smarthealthit.org/Patient') as response
    JOIN
      json_each(response.json, '$.entry') AS entries
      ON 1=1
    LIMIT 5;`.trimStart())
const rows = ref([])
const columns = ref([])
const message = ref('')

let db

onMounted(async () => {
    db = new Database();
})

// #region handleQuery
async function handleQuery() {
  try {
    rows.value = []
    columns.value = []
    message.value = ''

    const result = await db.query(true, sql.value).then(r => r.rows);
    if (result.length > 0) {
      rows.value = result
      columns.value = Object.keys(result[0])
    } else {
      message.value = 'Query executed successfully (no result rows).'
    }
  } catch (err) {
    message.value = `Error: ${err.message}`
  }
}
// #endregion handleQuery
</script>

<style scoped>
.repl {
  font-family: sans-serif;
  max-width: 700px;
  margin: auto;
  padding: 1rem;
}

.query {
  width: 100%;
  height: 40vh;
  padding: 0.6rem;
  margin-bottom: 1rem;
}

.output {
  margin-top: 1rem;
}

table {
  border-collapse: collapse;
  width: 100%;
}

th, td {
  border: 1px solid #ccc;
  padding: 0.5rem;
}
</style>
