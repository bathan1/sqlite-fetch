import type { Migration } from "kysely";

export const Migration2025_07_31: Migration = {
    async up(db) {
        await db.schema
            .alterTable("users")
            .dropColumn("name")
            .execute();
        await db.schema
            .alterTable("users")
            .addColumn("first_name", "text", col => col.notNull())
            .execute();
        await db.schema
            .alterTable("users")
            .addColumn("last_name", "text", col => col.notNull())
            .execute();
    },
    
    async down(db) {
        const users = db.schema.alterTable("users");
        await users
            .dropColumn("last_name")
            .execute();
        await users
            .dropColumn("first_name")
            .execute();
        
        await users
            .addColumn("name", "text", col => col.notNull())
            .execute();
    }
}