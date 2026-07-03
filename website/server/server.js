import dotenv from "dotenv"
dotenv.config()

import express from "express"
import cors from "cors"
import { performance } from "perf_hooks"

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
    verifyPassword,
    checkBruteForce,
    recordFailedAttempt,
    resetFailedAttempts
} from "./authCore.js"
import { pool, runMigrations } from "./db.js"
import {
    sendAccountWelcomeEmail,
    sendNewsletterWelcomeEmail,
    sendPasswordChangedEmail,
    sendPasswordChangeCodeEmail
} from "./mail.js"
import adminRouter from "./admin.js"
import debugRouter from "./debug.js"
import gameAnalyticsRouter from "./gameAnalytics.js"
import tokenExchangeRouter from "./token-exchange.js"
import clientLoginRouter from "./client-login.js"
import gameApiRouter from "./game-api.js"
import emailCampaignsRouter from "./email-campaigns.js"
import { trackEvent } from "./analytics.js"
import { createRateLimit } from "./rateLimit.js"
import {
    parseCookies,
    clearSessionCookie,
    setCsrfCookie,
    csrfProtection,
    createSession,
    authenticate,
    sessionCookieName,
    sessionSecret,
    sessionDays,
    production
} from "./session.js"
import crypto from "crypto"
import multer from "multer"
import sharp from "sharp"
import path from "path"
import fs from "fs"

const AVATAR_DIR = process.env.AVATAR_DIR || path.resolve("public/avatars")
if (!fs.existsSync(AVATAR_DIR)) {
    fs.mkdirSync(AVATAR_DIR, { recursive: true })
}

const storage = multer.memoryStorage()
const upload = multer({
    storage,
    limits: { fileSize: 5 * 1024 * 1024 },
    fileFilter: (req, file, cb) => {
        const allowed = ["image/jpeg", "image/png", "image/webp"]
        if (!allowed.includes(file.mimetype)) {
            cb(new Error("only jpg, png, and webp allowed"))
            return
        }
        cb(null, true)
    }
})

const authRateLimit = createRateLimit({ windowMs: 60 * 1000, max: 10, name: "auth" })
const adminRateLimit = createRateLimit({ windowMs: 60 * 1000, max: 20, name: "admin" })
const newsletterRateLimit = createRateLimit({ windowMs: 60 * 1000, max: 5, name: "newsletter" })
const gameAnalyticsRateLimit = createRateLimit({ windowMs: 60 * 1000, max: 120, name: "game_analytics" })
const avatarRateLimit = createRateLimit({ windowMs: 60 * 1000, max: 3, name: "avatar" })
const feedbackRateLimit = createRateLimit({ windowMs: 60 * 1000, max: 5, name: "feedback" })
const passwordChangeRateLimit = createRateLimit({ windowMs: 60 * 60 * 1000, max: 5, name: "password_change" })
const downloadTrackRateLimit = createRateLimit({ windowMs: 60 * 1000, max: 10, name: "download_track" })

const app = express()
const port = Number(process.env.PORT || 3001)

if (production && sessionSecret === "development-only-change-me") {
    throw new Error("SESSION_SECRET is required in production")
}

app.set("trust proxy", 1)
app.use(cors({
    origin: process.env.APP_ORIGIN || "http://localhost:5173",
    credentials: true
}))
app.use(express.json({ limit: "32kb" }))
app.use("/avatars", express.static(AVATAR_DIR))

app.use((req, res, next) => {
    res.set("X-Content-Type-Options", "nosniff")
    res.set("X-Frame-Options", "DENY")
    res.set("Referrer-Policy", "strict-origin-when-cross-origin")
    res.set("Permissions-Policy", "camera=(), microphone=(), geolocation=(), interest-cohort=()")
    res.set("Content-Security-Policy", "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; font-src 'self'; connect-src 'self' https://mimita.fun; frame-ancestors 'none'")
    if (production) {
        res.set("Strict-Transport-Security", "max-age=31536000; includeSubDomains")
    }
    next()
})

app.use((req, res, next) => {
    if (!parseCookies(req)["csrf_token"]) {
        setCsrfCookie(res)
    }
    next()
})

app.use((req, res, next) => {
    if (req.path.startsWith("/api/game/")) return next()
    if (req.path.startsWith("/api/games/")) return next()
    if (req.path === "/api/auth/exchange-session") return next()
    if (req.path === "/api/auth/signout") return next()
    if (req.path.startsWith("/api/client-login/")) return next()
    if (req.headers["authorization"]?.startsWith("Bearer ")) return next()
    return csrfProtection(req, res, next)
})

const LOG_REQUESTS = process.env.LOG_REQUESTS !== "false"

app.use((req, res, next) => {
    if (!LOG_REQUESTS) return next()

    const start = performance.now()
    const timestamp = new Date().toISOString()

    res.on("finish", () => {
        const duration = Math.round((performance.now() - start) * 100) / 100
        const logData = {
            method: req.method,
            path: req.originalUrl,
            status: res.statusCode,
            duration: `${duration}ms`,
            timestamp
        }

        if (res.statusCode >= 400) {
            console.log(`[REQUEST ERROR] ${JSON.stringify(logData)}`)
        }
        else {
            console.log(`[REQUEST] ${JSON.stringify(logData)}`)
        }
    })

    next()
})

function detectImageType(buffer) {
    const sig = buffer.slice(0, 8).toString("hex")
    if (sig.startsWith("89504e470d0a1a0a")) return "png"
    if (sig.startsWith("ffd8")) return "jpeg"
    if (sig.startsWith("52494646")) {
        if (buffer.length > 12 && buffer.slice(8, 12).toString() === "WEBP") return "webp"
    }
    return null
}

function logAuth(action, value) {
    console.log(`[AUTH] ${action}=${value}`)
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


app.post("/api/auth/signup", authRateLimit, async (req, res, next) => {
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
        const verificationToken = createSixDigitCode()
        const verificationTokenHash = hashToken(verificationToken, sessionSecret)
        const result = await pool.query(
            `
            INSERT INTO users (
                username,
                username_key,
                email,
                password_hash,
                email_verification_token
            )
            VALUES ($1, $2, $3, $4, $5)
            RETURNING id, username, email, bio, avatar_url, avatar_updated_at, supporter_tier, email_notifications_enabled
            `,
            [username, usernameKey(username), email, passwordHash, verificationTokenHash]
        )
        const user = result.rows[0]

        await createSession(user.id, req, res)
        logAuth("signup", `success user_id=${user.id}`)
        await trackEvent("account_created", {
            event_data: {
                source: "website",
                username: user.username
            },
            user_id: user.id,
            ip_address: getClientIp(req)
        })
        await safelySend(
            "welcome_sent",
            () => sendAccountWelcomeEmail(email, username, verificationToken)
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
                message: "account already exists"
            })
        }

        logAuth("signup", "failed")
        next(error)
    }
})

app.post("/api/auth/signin", authRateLimit, async (req, res, next) => {
    try {
        const identifier = String(req.body.identifier || "").trim()

        const bruteForce = await checkBruteForce(identifier)
        if (bruteForce.locked) {
            logAuth("signin", "account_locked")
            return res.status(429).json({
                success: false,
                message: `account locked. try again in ${bruteForce.remaining}s`
            })
        }

        const result = await pool.query(
            `
            SELECT
                id,
                username,
                email,
                password_hash,
                bio,
                avatar_url,
                avatar_updated_at,
                supporter_tier,
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
            await recordFailedAttempt(identifier, getClientIp(req))
            await trackEvent("failed_login", {
                event_data: {
                    source: "website",
                    identifier_type: identifier.includes("@") ? "email" : "username"
                },
                ip_address: getClientIp(req)
            })
            return res.status(401).json({
                success: false,
                message: "invalid username/email or password"
            })
        }

        await resetFailedAttempts(identifier)
        await createSession(user.id, req, res)
        delete user.password_hash
        logAuth("signin", `success user_id=${user.id}`)
        await trackEvent("login", {
            event_data: {
                source: "website",
                username: user.username
            },
            user_id: user.id,
            ip_address: getClientIp(req)
        })

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
        await trackEvent("logout", {
            event_data: {
                source: "website",
                username: req.user.username
            },
            user_id: req.user.id,
            ip_address: getClientIp(req)
        })
        res.json({
            success: true,
            message: "Signed out"
        })
    }
    catch (error) {
        next(error)
    }
})

app.get("/api/auth/verify-email/:token", async (req, res, next) => {
    try {
        const token = String(req.params.token || "")
        const tokenHash = hashToken(token, sessionSecret)
        const result = await pool.query(
            `
            UPDATE users
            SET email_verified_at = NOW(), email_verification_token = NULL
            WHERE email_verification_token = $1
              AND email_verified_at IS NULL
            RETURNING id
            `,
            [tokenHash]
        )
        if (!result.rowCount) {
            return res.status(400).json({
                success: false,
                message: "invalid or expired verification link"
            })
        }
        logAuth("verify_email", `success user_id=${result.rows[0].id}`)
        res.json({ success: true, message: "email verified" })
    }
    catch (error) {
        next(error)
    }
})

app.post("/api/auth/resend-verification", authenticate, async (req, res, next) => {
    try {
        if (req.user.email_verified) {
            return res.json({ success: true, message: "email already verified" })
        }
        const code = createSixDigitCode()
        const codeHash = hashToken(code, sessionSecret)
        await pool.query(
            `UPDATE users SET email_verification_token = $1 WHERE id = $2`,
            [codeHash, req.user.id]
        )
        await sendAccountWelcomeEmail(req.user.email, req.user.username, code)
        logAuth("verify_email", `resent user_id=${req.user.id}`)
        res.json({ success: true, message: "verification code sent" })
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
            RETURNING username, bio, avatar_url, avatar_updated_at, supporter_tier
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

app.post("/api/account/avatar", authenticate, avatarRateLimit, upload.single("avatar"), async (req, res, next) => {
    try {
        if (!req.file) {
            return res.status(400).json({
                success: false,
                message: "no file provided"
            })
        }

        const imageType = detectImageType(req.file.buffer)
        if (!imageType) {
            return res.status(400).json({
                success: false,
                message: "invalid image file"
            })
        }

        const fileName = `user_${req.user.id}_${crypto.randomUUID()}.png`
        const filePath = path.join(AVATAR_DIR, fileName)

        const img = sharp(req.file.buffer, { limitInputPixels: 4_000_000 })
        img.resize(512, 512, { fit: "cover" })
        await img.png().toFile(filePath)

        const now = new Date().toISOString()
        const avatarUrl = `/avatars/${fileName}`

        const result = await pool.query(
            `
            UPDATE users
            SET avatar_url = $1, avatar_updated_at = $2, updated_at = $2
            WHERE id = $3
            RETURNING avatar_url, avatar_updated_at
            `,
            [avatarUrl, now, req.user.id]
        )

        if (req.user.avatar_url) {
            const oldFile = path.join(AVATAR_DIR, path.basename(req.user.avatar_url))
            if (fs.existsSync(oldFile)) {
                fs.unlinkSync(oldFile)
            }
        }

        res.json({
            success: true,
            avatar_url: result.rows[0].avatar_url,
            avatar_updated_at: result.rows[0].avatar_updated_at
        })
    }
    catch (error) {
        next(error)
    }
})

app.delete("/api/account/avatar", authenticate, async (req, res, next) => {
    try {
        if (req.user.avatar_url) {
            const oldFile = path.join(AVATAR_DIR, path.basename(req.user.avatar_url))
            if (fs.existsSync(oldFile)) {
                fs.unlinkSync(oldFile)
            }
        }

        await pool.query(
            `
            UPDATE users
            SET avatar_url = '', avatar_updated_at = NULL, updated_at = NOW()
            WHERE id = $1
            `,
            [req.user.id]
        )

        res.json({
            success: true,
            message: "avatar removed"
        })
    }
    catch (error) {
        next(error)
    }
})

app.get("/api/avatar/initials", (req, res) => {
    const name = String(req.query.name || "?").trim()
    const initial = name.length > 0 ? name[0].toUpperCase() : "?"
    const size = Math.min(Math.max(Number(req.query.size) || 64, 16), 512)
    const fontSize = Math.round(size * 0.45)
    const borderW = Math.max(Math.round(size * 0.04), 2)

    const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${size}" height="${size}" viewBox="0 0 ${size} ${size}">
        <defs>
            <linearGradient id="bg" x1="0" y1="0" x2="1" y2="1">
                <stop offset="0%" stop-color="#1a1a1a"/>
                <stop offset="100%" stop-color="#000000"/>
            </linearGradient>
        </defs>
        <rect width="${size}" height="${size}" rx="${size * 0.06}" fill="url(#bg)" stroke="#555555" stroke-width="${borderW}"/>
        <text x="50%" y="50%" dominant-baseline="central" text-anchor="middle"
              font-family="MingLiU, MingLiU-ExtB, MingLiU_HKSCS-ExtB, serif"
              font-size="${fontSize}" font-weight="400"
              fill="#bbbbbb">${initial}</text>
    </svg>`

    res.set("Content-Type", "image/svg+xml")
    res.set("Cache-Control", "public, max-age=86400")
    res.send(svg)
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

app.get("/api/users", async (req, res, next) => {
    try {
        const page = Math.max(1, Number(req.query.page) || 1)
        const limit = Math.min(Number(req.query.limit) || 50, 200)
        const offset = (page - 1) * limit
        const sort = req.query.sort || "newest"
        const search = String(req.query.search || "").trim()

        let whereClause = "WHERE deleted_at IS NULL"
        const params = []
        let paramIndex = 1

        if (search) {
            whereClause += ` AND (username ILIKE $${paramIndex} OR bio ILIKE $${paramIndex})`
            params.push(`%${search}%`)
            paramIndex++
        }

        let orderClause
        switch (sort) {
            case "oldest":
                orderClause = "ORDER BY created_at ASC"
                break
            case "username_az":
                orderClause = "ORDER BY username ASC"
                break
            case "username_za":
                orderClause = "ORDER BY username DESC"
                break
            default:
                orderClause = "ORDER BY created_at DESC"
        }

        const countResult = await pool.query(
            `SELECT COUNT(*) FROM users ${whereClause}`, params
        )
        const total = Number(countResult.rows[0].count)

        params.push(limit, offset)
        const result = await pool.query(
            `SELECT username, bio, avatar_url, avatar_updated_at, supporter_tier, created_at
             FROM users ${whereClause} ${orderClause}
             LIMIT $${paramIndex} OFFSET $${paramIndex + 1}`,
            params
        )

        const users = result.rows.map(u => {
            if (!u.avatar_url) {
                const encoded = encodeURIComponent(u.username || "?")
                u.avatar_url = `/api/avatar/initials?name=${encoded}&size=128`
            }
            return u
        })

        res.json({
            success: true,
            users,
            total,
            pages: Math.ceil(total / limit),
            page,
            limit
        })
    }
    catch (error) {
        next(error)
    }
})

app.get("/api/users/:username", async (req, res, next) => {
    try {
        const result = await pool.query(
            `
            SELECT username, bio, avatar_url, avatar_updated_at, supporter_tier, created_at
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

        const u = result.rows[0]
        if (!u.avatar_url) {
            const encoded = encodeURIComponent(u.username || "?")
            u.avatar_url = `/api/avatar/initials?name=${encoded}&size=128`
        }

        res.json({
            success: true,
            user: u
        })
    }
    catch (error) {
        next(error)
    }
})

app.post(
    "/api/auth/password-change/request",
    passwordChangeRateLimit,
    authenticate,
    async (req, res, next) => {
        try {
            if (!req.user.email_verified) {
                return res.status(403).json({
                    success: false,
                    message: "verify your email before changing password"
                })
            }

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

app.use("/api/admin", adminRateLimit, adminRouter)
app.use("/api/admin/email-campaigns", adminRateLimit, emailCampaignsRouter)
app.use("/api/debug", debugRouter)
app.use("/api/game/analytics", gameAnalyticsRateLimit, gameAnalyticsRouter)
app.use("/api", gameApiRouter)

app.post("/api/admin/feedback", feedbackRateLimit, async (req, res, next) => {
    try {
        const { submitFeedback } = await import("./feedback.js")
        await submitFeedback({
            selectedPresets: req.body.selectedPresets,
            customFeedback: req.body.customFeedback,
            contactInfo: req.body.contactInfo,
            pageUrl: req.body.pageUrl,
            userId: req.body.userId
        })
        res.status(201).json({ success: true, message: "feedback submitted" })
    }
    catch (error) {
        next(error)
    }
})

/*
    TODO: Increment downloads_today when installer download starts.
    TODO: Increment accounts_created when signup succeeds.
    TODO: Increment first_match_played when user finishes first match.
    TODO: Increment returning_users when account logs in on a later day than creation.
    TODO: Increment donation_page_visits when donation page loads.
    TODO: Increment donations_total when donation succeeds.
    TODO: Track session length on disconnect/logout.
    TODO: Track retention calculations nightly.
    TODO: Postgres production setup.
    TODO: Authentication migration.
    TODO: Email notifications.
    TODO: Moderation workflow.
    TODO: Anti-spam protection.
    TODO: Rate limiting.
    TODO: Analytics aggregation jobs.
    TODO: Data retention policies.
*/

// ── Account Linking ──────────────────────────────────────────────────────────
// In-memory store for linking codes. Codes are 6 digits, expire after 5 minutes.
// Security: poll returns only claimed=true/false. Finalize requires one-time grant token.

const linkCodes = new Map()
const linkRateLimit = createRateLimit({ windowMs: 10 * 1000, max: 10, name: "link" })

function randomHex(bytes) {
    return crypto.randomBytes(bytes).toString("hex")
}

function generateLinkCode() {
    const code = String(Math.floor(100000 + Math.random() * 900000))
    const grantToken = randomHex(32)
    linkCodes.set(code, {
        code,
        grantToken,
        createdAt: Date.now(),
        claimed: false,
        userId: null,
        username: null
    })
    setTimeout(() => linkCodes.delete(code), 5 * 60 * 1000)
    return { code, grantToken }
}

app.post("/api/auth/link-code", (req, res) => {
    try {
        const { code, grantToken } = generateLinkCode()
        res.json({ success: true, code, grant_token: grantToken, url: "https://mimita.fun/link" })
    }
    catch (error) {
        res.status(500).json({ success: false, message: "failed to generate code" })
    }
})

app.post("/api/auth/link-claim", linkRateLimit, async (req, res, next) => {
    try {
        const token = parseCookies(req)[sessionCookieName]
        if (!token) {
            return res.status(401).json({ success: false, message: "sign in required" })
        }

        const code = String(req.body.code || "").trim()
        const entry = linkCodes.get(code)
        if (!entry) {
            return res.status(404).json({ success: false, message: "invalid or expired code" })
        }
        if (entry.claimed) {
            return res.status(400).json({ success: false, message: "code already used" })
        }

        const result = await pool.query(
            "SELECT id, username FROM users WHERE id = (SELECT user_id FROM sessions WHERE token_hash = $1 AND revoked_at IS NULL AND expires_at > NOW() LIMIT 1)",
            [hashToken(token, sessionSecret)]
        )
        if (!result.rowCount) {
            return res.status(401).json({ success: false, message: "session invalid" })
        }

        entry.claimed = true
        entry.userId = result.rows[0].id
        entry.username = result.rows[0].username

        res.json({ success: true, message: "account linked as " + result.rows[0].username })
    }
    catch (error) {
        next(error)
    }
})

app.get("/api/auth/link-poll", linkRateLimit, (req, res) => {
    const code = String(req.query.code || "").trim()
    const entry = linkCodes.get(code)

    if (!entry) {
        return res.json({ success: false, claimed: false })
    }
    if (entry.claimed) {
        return res.json({
            success: true,
            claimed: true,
            username: entry.username
        })
    }

    res.json({ success: true, claimed: false })
})

app.post("/api/auth/link-finalize", linkRateLimit, async (req, res, next) => {
    try {
        const code = String(req.body.code || "").trim()
        const grantToken = String(req.body.grant_token || "").trim()
        const entry = linkCodes.get(code)

        if (!entry || !entry.claimed) {
            return res.status(404).json({ success: false, message: "invalid or expired code" })
        }
        if (entry.grantToken !== grantToken) {
            return res.status(403).json({ success: false, message: "invalid grant token" })
        }

        // One-time use — delete immediately
        linkCodes.delete(code)

        // Create a new session for the linked user
        const token = createSecretToken()
        const tokenHash = hashToken(token, sessionSecret)
        await pool.query(
            `INSERT INTO sessions (user_id, token_hash, user_agent, ip_address, expires_at)
             VALUES ($1, $2, $3, $4, NOW() + ($5 * INTERVAL '1 day'))`,
            [entry.userId, tokenHash, "game-client", getClientIp(req), sessionDays]
        )

        res.json({
            success: true,
            user: {
                id: entry.userId,
                username: entry.username
            },
            session_token: token
        })
    }
    catch (error) {
        next(error)
    }
})
// ── End Account Linking ──────────────────────────────────────────────────────

app.use("/api/auth", tokenExchangeRouter)
app.use("/api/client-login", clientLoginRouter)

app.post("/api/track/download", downloadTrackRateLimit, async (req, res, next) => {
    try {
        await trackEvent("download", {
            event_data: {
                source: req.body.source || "website",
                platform: req.body.platform || "unknown"
            },
            user_id: req.user?.id || null,
            ip_address: getClientIp(req),
            page_url: "/download"
        })
        res.json({ success: true })
    }
    catch (error) {
        next(error)
    }
})

app.post("/api/newsletter", newsletterRateLimit, async (req, res, next) => {
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
                success: true,
                message: "joined newsletter"
            })
        }

        next(error)
    }
})

let gameVersion = { version: "1.0.0", release_date: "unknown", file_size_mb: 0, platform: "windows-64" }
const versionPath = path.resolve("server/version.json")
if (fs.existsSync(versionPath)) {
    try {
        gameVersion = JSON.parse(fs.readFileSync(versionPath, "utf8"))
    } catch (e) {
        console.log("[VERSION] Failed to load version.json:", e.message)
    }
}

app.get("/api/game/version", (req, res) => {
    res.json(gameVersion)
})

app.get("/api/update/latest-version", (req, res) => {
    res.json({
        version: gameVersion.version,
        release_date: gameVersion.release_date,
        download_url: "/api/download/latest",
        manifest_url: "/api/update/manifest/" + gameVersion.version,
        mandatory: false,
        changelog: "Initial release."
    })
})

app.get("/api/update/manifest/:version", (req, res) => {
    const manifestPath = path.resolve("server/manifests/" + req.params.version + ".json")
    if (fs.existsSync(manifestPath)) {
        res.sendFile(manifestPath)
    }
    else {
        res.status(404).json({ success: false, message: "manifest not found" })
    }
})

app.get(/^\/api\/download\/file\/(.+)/, (req, res) => {
    const safePath = String(req.params[0] || "")
    const filePath = path.resolve(safePath)
    const resolved = path.resolve(filePath)
    const gameDir = path.resolve(".")
    if (!resolved.startsWith(gameDir)) {
        return res.status(403).json({ success: false, message: "forbidden" })
    }
    if (fs.existsSync(resolved)) {
        return res.sendFile(resolved)
    }
    const rootPath = path.resolve("..", req.params[0] || "")
    const rootResolved = path.resolve(rootPath)
    const repoDir = path.resolve("..")
    if (!rootResolved.startsWith(repoDir)) {
        return res.status(403).json({ success: false, message: "forbidden" })
    }
    if (fs.existsSync(rootResolved)) {
        return res.sendFile(rootResolved)
    }
    res.status(404).json({ success: false, message: "file not found" })
})

app.get("/api/download/latest", (req, res) => {
    const filename = "MimitaSetup-" + gameVersion.version + ".exe"
    const filePath = path.resolve("server/downloads", filename)
    if (fs.existsSync(filePath)) {
        res.download(filePath, filename)
    }
    else {
        res.redirect("https://github.com/jorj1357/mimita-public/releases/latest")
    }
})

app.use((error, req, res, next) => {
    void req
    void next
    const logData = {
        type: error.constructor?.name || "Error",
        message: error.message,
        code: error.code || "unknown",
        stack: error.stack?.split("\n").slice(0, 3).join(" | ") || ""
    }
    console.log(`[SERVER ERROR] ${JSON.stringify(logData)}`)

    if (error.code === "ECONNREFUSED") {
        console.log("[SERVER] Database connection refused.")
        console.log("[SERVER] Ensure PostgreSQL is running and accessible.")
        console.log("[SERVER] Check .env for: DB_HOST, DB_PORT, DB_USER, DB_PASSWORD, DB_NAME")
        return res.status(500).json({
            success: false,
            message: "database connection failed. check server logs."
        })
    }

    res.status(500).json({
        success: false,
        message: "server error"
    })
})

async function start() {
    const startTime = Date.now()

    console.log("=".repeat(50))
    console.log("[STARTUP] Mimita Backend Server")
    console.log("[STARTUP] Environment:", production ? "production" : "development")
    console.log("[STARTUP] Port:", port)
    console.log("[STARTUP] Session days:", sessionDays)
    console.log("[STARTUP] CORS origin:", process.env.APP_ORIGIN || "http://localhost:5173")
    console.log("[STARTUP] Database host:", process.env.DB_HOST || "localhost")
    console.log("[STARTUP] Database name:", process.env.DB_NAME || "mimita_db")
    console.log("[STARTUP] Database user:", process.env.DB_USER || "mimita_user")
    console.log("[STARTUP] SMTP configured:", Boolean(process.env.SMTP_HOST))
    console.log("-".repeat(50))

    try {
        await runMigrations()
        console.log("[STARTUP] Migrations applied successfully")
    }
    catch (error) {
        console.log("[STARTUP] Migration failed:", error.message)
        console.log("[STARTUP] Server will start anyway. Run migrations separately if needed.")
    }

    // Periodic session cleanup: purge expired sessions every hour
    setInterval(async () => {
        try {
            const result = await pool.query(
                `DELETE FROM sessions WHERE expires_at < NOW() - INTERVAL '1 day'`
            )
            if (result.rowCount > 0) {
                logAuth("cleanup", `purged ${result.rowCount} expired sessions`)
            }
        } catch (e) {
            console.log(`[AUTH] session cleanup error: ${e.message}`)
        }
    }, 60 * 60 * 1000)

    // Run once at startup
    setTimeout(async () => {
        try {
            const result = await pool.query(
                `DELETE FROM sessions WHERE expires_at < NOW() - INTERVAL '1 day'`
            )
            console.log(`[AUTH] startup cleanup: purged ${result.rowCount} expired sessions`)
        } catch (e) {
            console.log(`[AUTH] startup cleanup error: ${e.message}`)
        }
    }, 5000)

    app.listen(port, () => {
        const elapsed = Date.now() - startTime
        console.log("-".repeat(50))
        console.log(`[STARTUP] Server ready on port ${port} (${elapsed}ms)`)
        console.log("[STARTUP] API base: http://localhost:" + port + "/api")
        console.log("[STARTUP] Debug:    http://localhost:" + port + "/api/debug/health")
        console.log("[STARTUP] Catalog:  http://localhost:" + port + "/api/debug/error-catalog")
        console.log("=".repeat(50))
    })
}

start().catch((error) => {
    console.log("[STARTUP] Fatal error during startup:")
    console.log("[STARTUP] ", error.message)

    if (error.code === "ECONNREFUSED") {
        console.log("[STARTUP] Could not connect to PostgreSQL.")
        console.log("[STARTUP] Possible causes:")
        console.log("[STARTUP]   1. PostgreSQL is not installed/running")
        console.log("[STARTUP]   2. Wrong host or port in .env")
        console.log("[STARTUP]   3. Firewall blocking port 5432")
        console.log("[STARTUP]   4. Invalid database credentials")
        console.log("[STARTUP] Expected connection:")
        console.log(`[STARTUP]   DATABASE_URL=postgresql://${process.env.DB_USER || "mimita_user"}:<password>@${process.env.DB_HOST || "localhost"}:${process.env.DB_PORT || 5432}/${process.env.DB_NAME || "mimita_db"}`)
        console.log("[STARTUP] Or set individual env vars:")
        console.log("[STARTUP]   DB_HOST, DB_PORT, DB_NAME, DB_USER, DB_PASSWORD")
    }
    else if (error.code === "28P01") {
        console.log("[STARTUP] Invalid database password.")
        console.log("[STARTUP] Check DB_PASSWORD in .env")
    }
    else if (error.code === "42P01") {
        console.log("[STARTUP] Missing tables. Run: npm run migrate")
    }
    else {
        console.log("[STARTUP] Unhandled error:", error.stack)
    }

    process.exitCode = 1
})
