import { access } from "node:fs/promises";
import { exit } from "node:process";

export async function checkExtensionExists() {
    const isExtensionMade = await access("./libyartssql.so")
        .then(() => true)
        .catch(() => false);
    if (!isExtensionMade) {
        console.error("You don't have the extension built lol");
        exit(1);
    } else {
        console.log("Extension found");
    }
}
