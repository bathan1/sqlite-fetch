import type { Migration } from "kysely";
import { Migration2025_07_30 } from "@/public/migrations/2025-07-30";
import { Migration2025_07_31 } from "@/public/migrations/2025-07-31";

export const migrations: Record<string, Migration> = {
    "2025-07-30": Migration2025_07_30,
    "2025-07-31": Migration2025_07_31
}