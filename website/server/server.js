import dotenv from "dotenv"
dotenv.config()

import express from "express"
import cors from "cors"

import {
    createSecretToken,
    createSixDigitCode,
    getClientIp,
    hashPassword,
    hashToken,
    normalizeEmail,
    usernameKey,
    validateEmail,
    validatePassword,
    validateUsername,
    verifyPassword
} from "./authCore.js"
import { pool, runMigrations } from "./db.js"
import {
    sendAccountWelcomeEmail,
    sendNewsletterWelcomeEmail,
    sendPasswordChangedEmail,
    sendPasswordChangeCodeEmail
} from "./mail.js"

const app = express()
const port = Number(process.env.PORT || 3001)
const sessionCookieName =
    process.env.SESSION_COOKIE_NAME || "mimita_session"
const sessionSecret =
    process.env.SESSION_SECRET || "development-only-change-me"
const sessionDays = Number(process.env.SESSION_DAYS || 30)
const production = process.env.NODE_ENV === "production"

if (production && sessionSecret === "development-only-change-me") {
    throw new Error("SESSION_SECRET is required in production")
}

app.set("trust proxy", 1)
app.use(cors({
    origin: process.env.APP_ORIGIN || "http://localhost:5173",
    credentials: true
}))
app.use(express.json({ limit: "32kb" }))

function logAuth(action, value) {
    console.log(`[AUTH] ${action}=${value}`)
}

function parseCookies(req) {
    const result = {}

    for (const pair of String(req.headers.cookie || "").split(";")) {
        const separator = pair.indexOf("=")

        if (separator === -1) {
            continue
        }

        const key = pair.slice(0, separator).trim()
        const value = pair.slice(separator + 1).trim()
        result[key] = decodeURIComponent(value)
    }

    return result
}

function setSessionCookie(res, token) {
    res.cookie(sessionCookieName, token, {
        httpOnly: true,
        secure: production,
        sameSite: "lax",
        maxAge: sessionDays * 24 * 60 * 60 * 1000,
        path: "/"
    })
}

function clearSessionCookie(res) {
    res.clearCookie(sessionCookieName, {
        httpOnly: true,
        secure: production,
        sameSite: "lax",
        path: "/"
    })
}

async function safelySend(label, send) {
    try {
        await send()
        logAuth("mail", label)
    }
    catch (error) {
        logAuth("mail", `${label}_failed`)
        console.error(error)
    }
}

async function createSession(userId, req, res) {
    const token = createSecretToken()
    const tokenHash = hashToken(token, sessionSecret)

    await pool.query(
        `
        INSERT INTO sessions (
            user_id,
            token_hash,
            user_agent,
            ip_address,
            expires_at
        )
        VALUES ($1, $2, $3, $4, NOW() + ($5 * INTERVAL '1 day'))
        `,
        [
            userId,
            tokenHash,
            req.get("user-agent") || "unknown",
            getClientIp(req),
            sessionDays
        ]
    )

    setSessionCookie(res, token)
}

async function authenticate(req, res, next) {
    try {
        const token = parseCookies(req)[sessionCookieName]

        if (!token) {
            return res.status(401).json({
                success: false,
                message: "sign in required"
            })
        }

        const result = await pool.query(
            `
            SELECT
                s.id AS session_id,
                u.id,
                u.username,
                u.email,
                u.bio,
                u.email_notifications_enabled
            FROM sessions s
            JOIN users u ON u.id = s.user_id
            WHERE s.token_hash = $1
              AND s.revoked_at IS NULL
              AND s.expires_at > NOW()
              AND u.deleted_at IS NULL
            LIMIT 1
            `,
            [hashToken(token, sessionSecret)]
        )

        if (!result.rowCount) {
            clearSessionCookie(res)
            return res.status(401).json({
                success: false,
                message: "session expired"
            })
        }

        req.user = result.rows[0]
        req.sessionTokenHash = hashToken(token, sessionSecret)
        next()
    }
    catch (error) {
        next(error)
    }
}

app.post("/api/auth/signup", async (req, res, next) => {
    try {
        const usernameValidation = validateUsername(req.body.username)
        const emailValidation = validateEmail(req.body.email)
        const passwordValidation = validatePassword(req.body.password)

        if (!usernameValidation.ok) {
            return res.status(400).json({
                success: false,
                message: usernameValidation.message
            })
        }

        if (!emailValidation.ok) {
            return res.status(400).json({
                success: false,
                message: emailValidation.message
            })
        }

        if (!passwordValidation.ok) {
            return res.status(400).json({
                success: false,
                message: passwordValidation.message
            })
        }

        const username = usernameValidation.value
        const email = emailValidation.value
        const passwordHash = await hashPassword(req.body.password)
        const result = await pool.query(
            `
            INSERT INTO users (
                username,
                username_key,
                email,
                password_hash
            )
            VALUES ($1, $2, $3, $4)
            RETURNING id, username, email, bio, email_notifications_enabled
            `,
            [username, usernameKey(username), email, passwordHash]
        )
        const user = result.rows[0]

        await createSession(user.id, req, res)
        logAuth("signup", `success user_id=${user.id}`)
        await safelySend(
            "welcome_sent",
            () => sendAccountWelcomeEmail(email, username)
        )

        return res.status(201).json({
            success: true,
            user
        })
    }
    catch (error) {
        if (error.code === "23505") {
            logAuth("signup", "duplicate")
            return res.status(409).json({
                success: false,
                message: error.constraint?.includes("username")
                    ? "username already exists"
                    : "email already exists"
            })
        }

        logAuth("signup", "failed")
        next(error)
    }
})

app.post("/api/auth/signin", async (req, res, next) => {
    try {
        const identifier = String(req.body.identifier || "").trim()
        const result = await pool.query(
            `
            SELECT
                id,
                username,
                email,
                password_hash,
                bio,
                email_notifications_enabled
            FROM users
            WHERE deleted_at IS NULL
              AND (
                  username_key = $1
                  OR email = $2
              )
            LIMIT 1
            `,
            [usernameKey(identifier), normalizeEmail(identifier)]
        )
        const user = result.rows[0]

        if (
            !user ||
            !(await verifyPassword(req.body.password, user.password_hash))
        ) {
            logAuth("signin", "invalid_credentials")
            return res.status(401).json({
                success: false,
                message: "invalid username/email or password"
            })
        }

        await createSession(user.id, req, res)
        delete user.password_hash
        logAuth("signin", `success user_id=${user.id}`)

        res.json({
            success: true,
            user
        })
    }
    catch (error) {
        logAuth("signin", "failed")
        next(error)
    }
})

app.post("/api/auth/signout", authenticate, async (req, res, next) => {
    try {
        await pool.query(
            `
            UPDATE sessions
            SET revoked_at = NOW()
            WHERE token_hash = $1
            `,
            [req.sessionTokenHash]
        )
        clearSessionCookie(res)
        logAuth("signout", `success user_id=${req.user.id}`)
        res.json({
            success: true,
            message: "Signed out"
        })
    }
    catch (error) {
        next(error)
    }
})

app.get("/api/auth/me", authenticate, (req, res) => {
    res.json({
        success: true,
        user: req.user
    })
})

app.patch("/api/account/profile", authenticate, async (req, res, next) => {
    try {
        const bio = String(req.body.bio || "").trim()

        if (bio.length > 500) {
            return res.status(400).json({
                success: false,
                message: "bio must be 500 characters or less"
            })
        }

        const result = await pool.query(
            `
            UPDATE users
            SET bio = $1, updated_at = NOW()
            WHERE id = $2
            RETURNING username, bio
            `,
            [bio, req.user.id]
        )

        res.json({
            success: true,
            profile: result.rows[0]
        })
    }
    catch (error) {
        next(error)
    }
})

app.patch(
    "/api/account/notification-preferences",
    authenticate,
    async (req, res, next) => {
        try {
            if (typeof req.body.emailNotifications !== "boolean") {
                return res.status(400).json({
                    success: false,
                    message: "emailNotifications must be true or false"
                })
            }

            await pool.query(
                `
                UPDATE users
                SET
                    email_notifications_enabled = $1,
                    updated_at = NOW()
                WHERE id = $2
                `,
                [req.body.emailNotifications, req.user.id]
            )

            res.json({
                success: true,
                emailNotifications: req.body.emailNotifications
            })
        }
        catch (error) {
            next(error)
        }
    }
)

app.get("/api/users/:username", async (req, res, next) => {
    try {
        const result = await pool.query(
            `
            SELECT username, bio
            FROM users
            WHERE username_key = $1
              AND deleted_at IS NULL
            LIMIT 1
            `,
            [usernameKey(req.params.username)]
        )

        if (!result.rowCount) {
            return res.status(404).json({
                success: false,
                message: "user not found"
            })
        }

        res.json({
            success: true,
            user: result.rows[0]
        })
    }
    catch (error) {
        next(error)
    }
})

app.post(
    "/api/auth/password-change/request",
    authenticate,
    async (req, res, next) => {
        try {
            const code = createSixDigitCode()
            const codeHash = hashToken(code, sessionSecret)

            await pool.query(
                `
                UPDATE password_change_codes
                SET used_at = NOW()
                WHERE user_id = $1
                  AND used_at IS NULL
                `,
                [req.user.id]
            )

            await pool.query(
                `
                INSERT INTO password_change_codes (
                    user_id,
                    code_hash,
                    expires_at,
                    request_ip,
                    request_user_agent
                )
                VALUES (
                    $1,
                    $2,
                    NOW() + INTERVAL '10 minutes',
                    $3,
                    $4
                )
                `,
                [
                    req.user.id,
                    codeHash,
                    getClientIp(req),
                    req.get("user-agent") || "unknown"
                ]
            )

            await sendPasswordChangeCodeEmail(req.user.email, code)
            logAuth("password_change", `code_sent user_id=${req.user.id}`)
            res.json({
                success: true,
                message: "verification code sent"
            })
        }
        catch (error) {
            logAuth("password_change", "request_failed")
            next(error)
        }
    }
)

app.post(
    "/api/auth/password-change/verify",
    authenticate,
    async (req, res, next) => {
        try {
            const code = String(req.body.code || "").trim()

            if (!/^\d{6}$/.test(code)) {
                return res.status(400).json({
                    success: false,
                    message: "enter the 6-digit code"
                })
            }

            const result = await pool.query(
                `
                UPDATE password_change_codes
                SET verified_at = NOW()
                WHERE id = (
                    SELECT id
                    FROM password_change_codes
                    WHERE user_id = $1
                      AND code_hash = $2
                      AND used_at IS NULL
                      AND expires_at > NOW()
                    ORDER BY created_at DESC
                    LIMIT 1
                )
                RETURNING id
                `,
                [req.user.id, hashToken(code, sessionSecret)]
            )

            if (!result.rowCount) {
                logAuth("password_change", "invalid_code")
                return res.status(400).json({
                    success: false,
                    message: "invalid or expired code"
                })
            }

            logAuth("password_change", `code_verified user_id=${req.user.id}`)
            res.json({
                success: true,
                message: "code verified"
            })
        }
        catch (error) {
            next(error)
        }
    }
)

app.post(
    "/api/auth/password-change/finalize",
    authenticate,
    async (req, res, next) => {
        const client = await pool.connect()

        try {
            const passwordValidation = validatePassword(req.body.newPassword)

            if (!passwordValidation.ok) {
                return res.status(400).json({
                    success: false,
                    message: passwordValidation.message
                })
            }

            if (req.body.newPassword !== req.body.confirmNewPassword) {
                return res.status(400).json({
                    success: false,
                    message: "new passwords do not match"
                })
            }

            await client.query("BEGIN")
            const userResult = await client.query(
                `
                SELECT password_hash
                FROM users
                WHERE id = $1
                  AND deleted_at IS NULL
                FOR UPDATE
                `,
                [req.user.id]
            )
            const oldPasswordMatches = await verifyPassword(
                req.body.oldPassword,
                userResult.rows[0]?.password_hash
            )

            if (!oldPasswordMatches) {
                await client.query("ROLLBACK")
                logAuth("password_change", "invalid_old_password")
                return res.status(401).json({
                    success: false,
                    message: "old password is incorrect"
                })
            }

            const codeResult = await client.query(
                `
                SELECT id
                FROM password_change_codes
                WHERE user_id = $1
                  AND verified_at IS NOT NULL
                  AND used_at IS NULL
                  AND expires_at > NOW()
                ORDER BY created_at DESC
                LIMIT 1
                FOR UPDATE
                `,
                [req.user.id]
            )

            if (!codeResult.rowCount) {
                await client.query("ROLLBACK")
                return res.status(400).json({
                    success: false,
                    message: "verify a current password change code first"
                })
            }

            const newHash = await hashPassword(req.body.newPassword)
            await client.query(
                `
                UPDATE users
                SET password_hash = $1, updated_at = NOW()
                WHERE id = $2
                `,
                [newHash, req.user.id]
            )
            await client.query(
                `
                UPDATE password_change_codes
                SET used_at = NOW()
                WHERE id = $1
                `,
                [codeResult.rows[0].id]
            )
            await client.query(
                `
                UPDATE sessions
                SET revoked_at = NOW()
                WHERE user_id = $1
                  AND token_hash <> $2
                  AND revoked_at IS NULL
                `,
                [req.user.id, req.sessionTokenHash]
            )
            await client.query("COMMIT")

            const changedAt = new Date().toISOString()
            logAuth("password_change", `success user_id=${req.user.id}`)

            if (req.user.email_notifications_enabled) {
                await safelySend(
                    "password_changed_sent",
                    () => sendPasswordChangedEmail(
                        req.user.email,
                        changedAt,
                        req.get("user-agent") || "unknown",
                        getClientIp(req)
                    )
                )
            }

            res.json({
                success: true,
                message: "password changed"
            })
        }
        catch (error) {
            await client.query("ROLLBACK")
            logAuth("password_change", "finalize_failed")
            next(error)
        }
        finally {
            client.release()
        }
    }
)

app.delete("/api/account", authenticate, async (req, res, next) => {
    const client = await pool.connect()

    try {
        const result = await client.query(
            `
            SELECT password_hash
            FROM users
            WHERE id = $1
              AND deleted_at IS NULL
            `,
            [req.user.id]
        )

        if (
            !result.rowCount ||
            !(await verifyPassword(
                req.body.password,
                result.rows[0].password_hash
            ))
        ) {
            logAuth("delete_account", "invalid_password")
            return res.status(401).json({
                success: false,
                message: "password is incorrect"
            })
        }

        await client.query("BEGIN")
        await client.query(
            `
            UPDATE users
            SET
                deleted_at = NOW(),
                updated_at = NOW(),
                email_notifications_enabled = FALSE
            WHERE id = $1
            `,
            [req.user.id]
        )
        await client.query(
            `
            UPDATE sessions
            SET revoked_at = NOW()
            WHERE user_id = $1
              AND revoked_at IS NULL
            `,
            [req.user.id]
        )
        await client.query("COMMIT")

        clearSessionCookie(res)
        logAuth("delete_account", `success user_id=${req.user.id}`)
        res.json({
            success: true,
            message: "account deleted"
        })
    }
    catch (error) {
        await client.query("ROLLBACK")
        logAuth("delete_account", "failed")
        next(error)
    }
    finally {
        client.release()
    }
})

app.post("/api/newsletter", async (req, res, next) => {
    try {
        const validation = validateEmail(req.body.email)

        if (!validation.ok) {
            return res.status(400).json({
                success: false,
                message: validation.message
            })
        }

        await pool.query(
            `
            INSERT INTO newsletter (email)
            VALUES ($1)
            `,
            [validation.value]
        )

        await safelySend(
            "newsletter_welcome_sent",
            () => sendNewsletterWelcomeEmail(validation.value)
        )
        res.json({
            success: true,
            message: "joined newsletter"
        })
    }
    catch (error) {
        if (error.code === "23505") {
            return res.json({
                success: false,
                alreadySubscribed: true,
                message: "email already signed up"
            })
        }

        next(error)
    }
})

app.use((error, req, res, next) => {
    void req
    void next
    console.error(error)
    res.status(500).json({
        success: false,
        message: "server error"
    })
})

async function start() {
    await runMigrations()
    app.listen(port, () => {
        console.log(`server running on port ${port}`)
    })
}

start().catch((error) => {
    console.error("database init failed")
    console.error(error)
    process.exitCode = 1
})
