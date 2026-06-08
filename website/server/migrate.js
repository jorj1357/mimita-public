import dotenv from "dotenv"
dotenv.config()

import { pool, runMigrations } from "./db.js"

try {
    await runMigrations()
    console.log("database migrations complete")
}
catch (error) {
    console.error("database migration failed")
    console.error(error)
    process.exitCode = 1
}
finally {
    await pool.end()
}
