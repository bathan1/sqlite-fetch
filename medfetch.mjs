import { createInterface } from 'node:readline/promises';
import { stdin as input, stdout as output } from 'node:process';
import OpenAI from 'openai';
import { DatabaseSync } from 'node:sqlite';

const openai = new OpenAI({
    apiKey: process.env.OPENAI_API_KEY
});

const db = new DatabaseSync('medfetch.db');
db.prepare('pragma foreign_keys = ON').run();
const schema = () => db.prepare("select * from sqlite_master;").all();

const rl = createInterface({ input, output });

/** Strip code fences that models sometimes emit @param {string} s */
function stripCodeFences(s) {
    return s.replace(/```(\w+)?/g, '');
}

/** 
 * Drain complete SQL statements (semicolon-terminated) from a buffer.
 * Ignores semicolons inside single/double-quoted strings and inside
 * -- line comments or /* block comments *\/.
 * Returns { statements, remainder }.
 * 
 * @param {string} buffer
 */
function drainStatements(buffer) {
    let stmts = [];
    let cur = '';
    let inSingle = false;
    let inDouble = false;
    let inLineComment = false;
    let inBlockComment = false;
    for (let i = 0; i < buffer.length; i++) {
        const ch = buffer[i];
        const next = buffer[i + 1];

        if (inLineComment) {
            cur += ch;
            if (ch === '\n') inLineComment = false;
            continue;
        }
        if (inBlockComment) {
            cur += ch;
            if (ch === '*' && next === '/') {
                cur += next;
                i++;
                inBlockComment = false;
            }
            continue;
        }
        if (!inSingle && !inDouble) {
            if (ch === '-' && next === '-') { cur += ch + next; i++; inLineComment = true; continue; }
            if (ch === '/' && next === '*') { cur += ch + next; i++; inBlockComment = true; continue; }
        }

        if (!inDouble && ch === "'") {
            cur += ch;
            if (inSingle) {
                // Handle escaped '' inside single-quoted strings
                if (buffer[i + 1] === "'") { cur += "'"; i++; } else { inSingle = false; }
            } else {
                inSingle = true;
            }
            continue;
        }

        if (!inSingle && ch === '"') {
            cur += ch;
            if (inDouble) {
                // Handle escaped "" inside double-quoted identifiers
                if (buffer[i + 1] === '"') { cur += '"'; i++; } else { inDouble = false; }
            } else {
                inDouble = true;
            }
            continue;
        }

        cur += ch;

        if (!inSingle && !inDouble && ch === ';') {
            const stmt = stripCodeFences(cur).trim();
            if (stmt !== ';' && stmt !== '') {
                stmts.push(stmt);
            }
            cur = '';
        }
    }
    return { statements: stmts, remainder: cur };
}

async function main() {
    while (true) {
        const answer = (await rl.question('medfetch> ')).trim();
        if (answer === '.exit') break;
        if (answer === '') continue;

        const sqlStream = await openai.responses.create({
            model: 'gpt-4.1',
            instructions:
                "Write the best corresponding SQL queries from the user's natural language request. " +
                `Syntax is SQLite, and the current state is: ${schema()}\n`,
            input: [{ role: 'user', content: answer }],
            stream: true,
        });

        let headerPrinted = false;
        let buffer = '';

        try {
            for await (const event of sqlStream) {
                if (event.type === 'response.created' && !headerPrinted) {
                    process.stdout.write('chat> ');
                    headerPrinted = true;
                } else if (event.type === 'response.output_text.delta') {
                    buffer += event.delta;

                    // As we get deltas, flush any completed statements
                    const { statements, remainder } = drainStatements(buffer);
                    buffer = remainder;

                    for (const stmt of statements) {
                        await runSql(stmt);
                    }
                } else if (event.type === 'response.completed') {
                    // Flush any final statement (even if no trailing ;)
                    const leftover = stripCodeFences(buffer).trim();
                    if (leftover) {
                        await runSql(leftover);
                    }
                    process.stdout.write('\n');
                    buffer = '';
                }
            }
        } catch (err) {
            console.error('\n[stream error]', err?.message ?? err);
            // Optionally: try flushing any safe complete statements we already saw
            const { statements } = drainStatements(buffer);
            for (const stmt of statements) {
                await runSql(stmt);
            }
            buffer = '';
        }
    }

    rl.close();
}

/** Execute a single SQL statement. Prints SELECTs as a table. */
async function runSql(stmt) {
    try {
        // Remove trailing semicolon (db.prepare handles fine either way, but tidy)
        const clean = stmt.trim().replace(/;+\s*$/, '');

        if (/^\s*select\b/i.test(clean)) {
            const rows = db.prepare(clean).all();
            if (rows.length === 0) {
                console.log('(no rows)');
            } else {
                console.table(rows);
            }
        } else {
            db.exec(clean);
            // Optional: Show changes for DML
            if (/^\s*(insert|update|delete)\b/i.test(clean)) {
                // better-sqlite3 exposes changes only via prepared run(); exec runs many statements.
                const info = db.prepare(clean).run(); // re-run via prepared to get info; or just skip this
                console.log(`${info.changes ?? 0} row(s) affected.`);
            }
        }
    } catch (e) {
        console.error('[sqlite error]', e.message);
        // You may want to log the statement for debugging:
        // console.error('Failed SQL:', stmt);
    }
}

main().catch((e) => console.error(e));