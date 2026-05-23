import dotenv from "dotenv"

import express from "express"

import cors from "cors"

import pg from "pg"

import fs from "fs"

import path from "path"

import nodemailer from "nodemailer"

import { fileURLToPath } from "url"

dotenv.config()

const { Pool } = pg

const app = express()

app.use(cors())

app.use(express.json())

const __filename =
    fileURLToPath(import.meta.url)

const __dirname =
    path.dirname(__filename)

const pool = new Pool({

    user: process.env.DB_USER,

    host: process.env.DB_HOST,

    database: process.env.DB_NAME,

    password: process.env.DB_PASSWORD,

    port: process.env.DB_PORT
})

const transporter = nodemailer.createTransport({

    host: process.env.SMTP_HOST,

    port: process.env.SMTP_PORT,

    secure: false,

    auth: {

        user: process.env.SMTP_USER,

        pass: process.env.SMTP_PASS
    }
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

    }
    catch (err) {

        console.log("database init failed")

        console.log(err)
    }
}

init()

function getTimestamp() {

    return new Date().toLocaleTimeString()
}

async function sendWelcomeEmail(email) {

    try {

        const htmlPath = path.join(
            __dirname,
            "emailTemplates",
            "welcome.html"
        )

        const html =
            fs.readFileSync(
                htmlPath,
                "utf8"
            )

        await transporter.sendMail({

            from:
                `"MiMITA" <hello@mimita.fun>`,

            to: email,

            subject:
                "Welcome to MiMITA",

            html
        })

        console.log(
            `[${getTimestamp()}] welcome email sent -> ${email}`
        )

    }
    catch (err) {

        console.log(
            `[${getTimestamp()}] failed sending email`
        )

        console.log(err)
    }
}

app.post("/api/newsletter", async (req, res) => {

    try {

        const { email } = req.body

        console.log(
            `[${getTimestamp()}] signup request -> ${email}`
        )

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

        console.log(
            `[${getTimestamp()}] email saved -> ${email}`
        )

        await sendWelcomeEmail(email)

        res.json({

            success: true,

            message: "joined newsletter"
        })

    }
    catch (err) {

        console.log(
            `[${getTimestamp()}] signup failed`
        )

        console.log(err)

        if (err.code === "23505") {

            return res.json({

                success: false,

                alreadySubscribed: true,

                message:
                    "email already signed up"
            })
        }

        res.status(500).json({

            success: false,

            message: "server error"
        })
    }
})

app.listen(process.env.PORT, () => {

    console.log(
        `[${getTimestamp()}] server running on port ${process.env.PORT}`
    )
})