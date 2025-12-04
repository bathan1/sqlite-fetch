import { expect, describe, it, beforeAll } from "vitest";
import Database from "better-sqlite3";
import { checkExtensionExists } from "./common.js";

const CREATE_TODOS_TABLE = (completedType) =>
`drop table if exists todos;
create virtual table todos using fetch (
    id int,
    "userId" int,
    title text,
    completed ${completedType},
    url text default 'https://jsonplaceholder.typicode.com/todos'
);`;

describe("Usual queries", () => {
    beforeAll(checkExtensionExists);
    const db = new Database().loadExtension("./libyarts");
    it("maps booleans to text", () => {
        const todos = db
            .exec(CREATE_TODOS_TABLE("text"))
            .prepare(`select * from todos`)
            .all();
        expect(todos.length).toBeGreaterThanOrEqual(1);
        todos.forEach((row) => {
            expect(typeof row.id).toBe("number");
            expect(typeof row.userId).toBe("number");
            expect(typeof row.title).toBe("string");
            expect(typeof row.completed).toBe("string");

            if (row.completed === "true" || row.completed === "false") {}
            else {
                expect(
                    `Didn't expect that "completed" column: ${row.completed}`,
                ).toEqual(0);
            }
        });
    });

    it("maps booleans to ints", () => {
        const todos = db
            .exec(CREATE_TODOS_TABLE("int"))
            .prepare(`select * from todos`)
            .all();
        expect(todos.length).toBeGreaterThanOrEqual(1);
        todos.forEach((row) => {
            expect(typeof row.id).toBe("number");
            expect(typeof row.userId).toBe("number");
            expect(typeof row.title).toBe("string");
            expect(typeof row.completed).toBe("number");
            if (row.completed === 1 || row.completed === 0) {}
            else {
                expect(
                    `Didn't expect that "completed" column: ${row.completed}`,
                ).toEqual(0);
            }
        });
    });
});
