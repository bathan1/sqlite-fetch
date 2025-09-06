import type { Migration } from "kysely";

export const Migration2025_07_30: Migration = {
    async up(db) {
        await db.schema
            .createTable("users")
            .ifNotExists()
            .addColumn("id", "text", col => col.primaryKey())
            .addColumn("name", "text", col => col.notNull())
            .addColumn("age", "integer", col => col.notNull())
            .execute();
    },
    async down(db) {
        await db.schema.dropTable("users").ifExists().execute();
    }
}