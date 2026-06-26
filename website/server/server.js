import dotenv from "dotenv"
dotenv.config()

import express from "express"
import cors from "cors"
import { performance } from "perf_hooks"

import {
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
import { trackEvent } from "./analytics.js"
import { createRateLimit } from "./rateLimit.js"
import {
    parseCookies,
    clearSessionCookie,
    setCsrfCookie,
    csrfProtection,
    createSession,
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

const AVATAR_DIR = path.resolve("public/avatars")
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
                u.avatar_url,
                u.supporter_tier,
                u.role,
                u.email_notifications_enabled,
                u.email_verified_at IS NOT NULL AS email_verified
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
            RETURNING id, username, email, bio, avatar_url, supporter_tier, email_notifications_enabled
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

        const bruteForce = checkBruteForce(identifier)
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
            recordFailedAttempt(identifier)
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

        resetFailedAttempts(identifier)
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
            RETURNING username, bio, avatar_url, supporter_tier
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

        const sizes = [64, 128, 256]
        const ext = ".webp"
        const baseName = `user_${req.user.id}_${crypto.randomUUID()}`
        const urls = []

        for (const size of sizes) {
            const fileName = `${baseName}_${size}${ext}`
            const filePath = path.join(AVATAR_DIR, fileName)

            const img = sharp(req.file.buffer, { limitInputPixels: 4_000_000 })
            const metadata = await img.metadata()
            const maxDim = Math.max(metadata.width || 1024, metadata.height || 1024)

            if (maxDim > 1024) {
                img.resize(1024, 1024, { fit: "inside", withoutEnlargement: true })
            }

            img.resize(size, size, { fit: "cover" })

            await img.toFile(filePath)
            urls.push(`/avatars/${fileName}`)
        }

        const avatarUrl = urls.find(u => u.includes("_256")) || urls[0]

        const result = await pool.query(
            `
            UPDATE users
            SET avatar_url = $1, updated_at = NOW()
            WHERE id = $2
            RETURNING avatar_url
            `,
            [avatarUrl, req.user.id]
        )

        res.json({
            success: true,
            avatar_url: result.rows[0].avatar_url,
            sizes: urls
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
            SELECT username, bio, avatar_url, supporter_tier, created_at
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
app.use("/api/debug", debugRouter)
app.use("/api/game/analytics", gameAnalyticsRateLimit, gameAnalyticsRouter)

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
const linkCodes = new Map()

function generateLinkCode() {
    const code = String(Math.floor(100000 + Math.random() * 900000))
    linkCodes.set(code, {
        code,
        createdAt: Date.now(),
        claimed: false,
        userId: null,
        sessionToken: null
    })
    // Expire old codes
    setTimeout(() => linkCodes.delete(code), 5 * 60 * 1000)
    return code
}

app.post("/api/auth/link-code", (req, res) => {
    const code = generateLinkCode()
    res.json({ success: true, code, url: "https://mimita.fun/link" })
})

app.post("/api/auth/link-claim", async (req, res, next) => {
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

        entry.claimed = true
        entry.sessionToken = token

        res.json({ success: true, message: "account linked" })
    }
    catch (error) {
        next(error)
    }
})

app.get("/api/auth/link-poll", (req, res) => {
    const code = String(req.query.code || "").trim()
    const entry = linkCodes.get(code)

    if (!entry) {
        return res.json({ success: false, claimed: false, message: "invalid or expired code" })
    }
    if (entry.claimed) {
        return res.json({
            success: true,
            claimed: true,
            session_token: entry.sessionToken
        })
    }

    res.json({ success: true, claimed: false, message: "waiting for browser..." })
})
// ── End Account Linking ──────────────────────────────────────────────────────

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
        mandatory: false,
        changelog: "Initial release."
    })
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
