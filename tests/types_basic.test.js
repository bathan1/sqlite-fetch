import {expect, describe, it, beforeAll} from "vitest";
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
);`

describe("Usual queries", () => {
    beforeAll(checkExtensionExists);

    const db = new Database().loadExtension("./libyarts");
    it("maps booleans to text", () => {
        db
            .exec(CREATE_TODOS_TABLE("text"))
            .prepare(`select * from todos`)
            .all()
            .forEach(row => {
                expect("completed" in row).toBe(true);
                if (row.completed === "true") {} 
                else if (row.completed === "false") {}
                else {
                    expect(`Didn't expect that "completed" column: ${row.completed}`)
                        .toEqual(0);
                }
            });
    });

    it("maps booleans to ints", () => {
        db
            .exec(CREATE_TODOS_TABLE("int"))
            .prepare(`select * from todos`)
            .all()
            .forEach(row => {
                expect("completed" in row).toBe(true);
                if (row.completed === 1) {} 
                else if (row.completed === 0) {}
                else {
                    expect(`Didn't expect that "completed" column: ${row.completed}`)
                        .toEqual(0);
                }
            });
    });
})
