import dotenv from "dotenv"
import express from "express"
import cors from "cors"
import pg from "pg"

dotenv.config()

const { Pool } = pg

const app = express()

app.use(cors())
app.use(express.json())

const pool = new Pool({

    user: process.env.DB_USER,

    host: process.env.DB_HOST,

    database: process.env.DB_NAME,

    password: process.env.DB_PASSWORD,

    port: process.env.DB_PORT
})

async function init() {

    try {

        await pool.query(`
            CREATE TABLE IF NOT EXISTS newsletter (

                id SERIAL PRIMARY KEY,

                email TEXT UNIQUE NOT NULL,

                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        `)

        console.log("newsletter table ready")

    } catch (err) {

        console.log("database init failed")

        console.log(err)
    }
}

init()

app.post("/api/newsletter", async (req, res) => {

    try {

        const { email } = req.body

        if (!email) {

            return res.status(400).json({

                success: false,

                message: "email required"
            })
        }

        await pool.query(
            `
            INSERT INTO newsletter (email)
            VALUES ($1)
            `,
            [email]
        )

        res.json({

            success: true
        })

    } catch (err) {

        console.log(err)

        // duplicate email
        if (err.code === "23505") {

            return res.json({

                success: false,

                alreadySubscribed: true,

                message: "email already signed up"
            })
        }

        // generic server error
        res.status(500).json({

            success: false,

            message: "server error"
        })
    }
})

app.listen(process.env.PORT, () => {

    console.log(
        `server running on port ${process.env.PORT}`
    )
})