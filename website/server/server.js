import dotenv from "dotenv"
dotenv.config()

import express from "express"
import cors from "cors"
import { performance } from "perf_hooks"

import {
    createSixDigitCode,
    createSecretToken,
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
app.use("/avatars", express.static(AVATAR_DIR, {
    maxAge: "1h",
    setHeaders: (res, filePath) => {
        if (filePath.endsWith(".png")) {
            res.set("Content-Type", "image/png")
        }
    }
}))

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
    if (req.path.startsWith("/api/client-login/")) return next()
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
            if (!(res.statusCode === 401 && req.path === "/api/auth/me")) {
                console.log(`[REQUEST ERROR] ${JSON.stringify(logData)}`)
            }
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

async function authenticate(req, res, next) {
    try {
        let token = parseCookies(req)[sessionCookieName]

        if (!token) {
            const authHeader = req.headers["authorization"]
            if (authHeader && authHeader.startsWith("Bearer ")) {
                token = authHeader.slice(7).trim()
            }
        }

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
                u.avatar_updated_at,
                u.supporter_tier,
                u.role,
                u.email_notifications_enabled,
                u.email_verified_at IS NOT NULL AS email_verified,
                u.achievements,
                u.created_at,
                u.email_visible,
                u.display_name,
                u.avatar_data,
                u.last_login_at
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

        if (req.body.password !== req.body.passwordConfirm) {
            return res.status(400).json({
                success: false,
                message: "passwords do not match"
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
            RETURNING id, username, email, bio, avatar_url, avatar_updated_at, supporter_tier, email_notifications_enabled, achievements, email_visible
            `,
            [username, usernameKey(username), email, passwordHash, verificationTokenHash]
        )
        const user = result.rows[0]

        await pool.query(`UPDATE users SET last_login_at = NOW(), display_name = $1 WHERE id = $2`,
            [username, user.id])
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
                avatar_updated_at,
                supporter_tier,
                email_notifications_enabled,
                achievements,
                email_visible
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
        await pool.query(`UPDATE users SET last_login_at = NOW() WHERE id = $1`, [user.id])
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
            RETURNING id, achievements
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
        const user = result.rows[0]
        if (!user.achievements || !user.achievements.includes("confirmed_email")) {
            await pool.query(
                `UPDATE users SET achievements = array_append(COALESCE(achievements, '{}'), 'confirmed_email') WHERE id = $1`,
                [user.id]
            )
            console.log("[ACHIEVEMENT] user_id=" + user.id + " achievement=confirmed_email awarded")
        }
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
            RETURNING username, bio, avatar_url, avatar_updated_at, supporter_tier, achievements, email_visible
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
            console.log("[AVATAR] user_id=" + req.user.id + " reason=no_file_provided")
            return res.status(400).json({
                success: false,
                message: "no file provided"
            })
        }

        console.log("[AVATAR] user_id=" + req.user.id + " type=" + req.file.mimetype + " size=" + req.file.size)

        const imageType = detectImageType(req.file.buffer)
        if (!imageType) {
            console.log("[AVATAR] user_id=" + req.user.id + " reason=invalid_image_type type=" + req.file.mimetype)
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

        console.log("[AVATAR] user_id=" + req.user.id + " status=saved file=" + fileName)
        res.json({
            success: true,
            avatar_url: result.rows[0].avatar_url,
            avatar_updated_at: result.rows[0].avatar_updated_at
        })
    }
    catch (error) {
        console.log("[AVATAR] user_id=" + req.user.id + " reason=error error=" + error.message)
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

app.patch("/api/account/email-visibility", authenticate, async (req, res, next) => {
    try {
        if (typeof req.body.emailVisible !== "boolean") {
            return res.status(400).json({
                success: false,
                message: "emailVisible must be true or false"
            })
        }

        const result = await pool.query(
            `
            UPDATE users
            SET email_visible = $1, updated_at = NOW()
            WHERE id = $2
            RETURNING email_visible
            `,
            [req.body.emailVisible, req.user.id]
        )

        res.json({
            success: true,
            email_visible: result.rows[0].email_visible
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
            SELECT username, bio, avatar_url, avatar_updated_at, supporter_tier, created_at, achievements,
                   CASE WHEN email_visible THEN email ELSE NULL END AS email
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

app.get("/api/users", async (req, res, next) => {
    try {
        const page = Math.max(1, Number(req.query.page) || 1)
        const limit = Math.min(100, Math.max(1, Number(req.query.limit) || 50))
        const offset = (page - 1) * limit
        const search = String(req.query.search || "").trim().toLowerCase()
        const sort = String(req.query.sort || "newest")

        let where = "WHERE deleted_at IS NULL"
        const params = []

        if (search) {
            params.push(`%${search}%`)
            where += ` AND (LOWER(username) LIKE $${params.length} OR LOWER(bio) LIKE $${params.length})`
        }

        const sortOrders = {
            oldest: "created_at ASC",
            username_az: "username ASC",
            username_za: "username DESC",
            achievements: "array_length(achievements, 1) DESC NULLS LAST, username ASC",
            least_achievements: "array_length(achievements, 1) ASC NULLS LAST, username ASC"
        }
        const order = sortOrders[sort] || "created_at DESC"

        const countResult = await pool.query(
            `SELECT COUNT(*) AS count FROM users ${where}`,
            params
        )
        const total = Number(countResult.rows[0].count)

        params.push(limit)
        params.push(offset)

        const result = await pool.query(
            `SELECT username, avatar_url, avatar_updated_at, bio, created_at, achievements
             FROM users
             ${where}
             ORDER BY ${order}
             LIMIT $${params.length - 1} OFFSET $${params.length}`,
            params
        )

        res.json({
            success: true,
            users: result.rows.map(({ achievements, ...u }) => ({
                ...u,
                achievement_count: Array.isArray(achievements) ? achievements.length : 0
            })),
            total,
            page,
            limit,
            pages: Math.ceil(total / limit)
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

// ── Account Profile ──────────────────────────────────────────────────────────

app.get("/api/profile", authenticate, async (req, res, next) => {
    try {
        const statsResult = await pool.query(
            `SELECT * FROM game_stats WHERE user_id = $1`,
            [req.user.id]
        )
        const settingsResult = await pool.query(
            `SELECT settings_json FROM user_settings WHERE user_id = $1`,
            [req.user.id]
        )
        res.json({
            success: true,
            profile: {
                id: req.user.id,
                username: req.user.username,
                email: req.user.email,
                bio: req.user.bio,
                display_name: req.user.display_name || req.user.username,
                avatar_url: req.user.avatar_url,
                avatar_updated_at: req.user.avatar_updated_at,
                avatar_data: req.user.avatar_data,
                supporter_tier: req.user.supporter_tier,
                role: req.user.role,
                achievements: req.user.achievements,
                email_verified: req.user.email_verified,
                email_visible: req.user.email_visible,
                created_at: req.user.created_at
            },
            stats: statsResult.rows[0] || null,
            settings: settingsResult.rows[0]?.settings_json || {}
        })
    }
    catch (error) {
        next(error)
    }
})

app.patch("/api/profile", authenticate, async (req, res, next) => {
    try {
        const { display_name, bio } = req.body
        const updates = []
        const params = []
        let idx = 1

        if (display_name !== undefined) {
            if (String(display_name).length > 32) {
                return res.status(400).json({ success: false, message: "display name too long" })
            }
            updates.push(`display_name = $${idx++}`)
            params.push(String(display_name).trim())
        }
        if (bio !== undefined) {
            if (String(bio).length > 500) {
                return res.status(400).json({ success: false, message: "bio too long" })
            }
            updates.push(`bio = $${idx++}`)
            params.push(String(bio).trim())
        }

        if (!updates.length) {
            return res.status(400).json({ success: false, message: "nothing to update" })
        }

        updates.push(`updated_at = NOW()`)
        params.push(req.user.id)

        const result = await pool.query(
            `UPDATE users SET ${updates.join(", ")} WHERE id = $${idx}
             RETURNING username, bio, display_name, avatar_url, avatar_updated_at, supporter_tier`,
            params
        )

        logAuth("profile_update", `user_id=${req.user.id}`)
        res.json({ success: true, profile: result.rows[0] })
    }
    catch (error) {
        next(error)
    }
})

// ── Avatar Data (JSON) ──────────────────────────────────────────────────────

app.get("/api/avatar/data", authenticate, async (req, res, next) => {
    try {
        res.json({
            success: true,
            avatar_data: req.user.avatar_data || null
        })
    }
    catch (error) {
        next(error)
    }
})

app.put("/api/avatar/data", authenticate, async (req, res, next) => {
    try {
        const avatarData = req.body.avatar_data
        if (!avatarData) {
            return res.status(400).json({ success: false, message: "avatar_data required" })
        }

        await pool.query(
            `UPDATE users SET avatar_data = $1, avatar_updated_at = NOW(), updated_at = NOW() WHERE id = $2`,
            [JSON.stringify(avatarData), req.user.id]
        )

        logAuth("avatar_data_update", `user_id=${req.user.id}`)
        res.json({ success: true, message: "avatar data saved" })
    }
    catch (error) {
        next(error)
    }
})

// ── Stats ────────────────────────────────────────────────────────────────────

app.get("/api/stats", authenticate, async (req, res, next) => {
    try {
        const result = await pool.query(
            `SELECT * FROM game_stats WHERE user_id = $1`,
            [req.user.id]
        )
        res.json({
            success: true,
            stats: result.rows[0] || {
                wins: 0, losses: 0, kills: 0, deaths: 0,
                games_played: 0, playtime_seconds: 0,
                highest_mmr: 5000, current_mmr: 5000,
                accuracy: 0, headshots: 0, best_kill_streak: 0
            }
        })
    }
    catch (error) {
        next(error)
    }
})

app.post("/api/stats", authenticate, async (req, res, next) => {
    try {
        const {
            won, kills, deaths, accuracy, headshots,
            damage_dealt, playtime_seconds, game_mode, map_name,
            match_id, participants
        } = req.body

        // Server-authoritative stats update
        const client = await pool.connect()
        try {
            await client.query("BEGIN")

            // Upsert game_stats
            const statsResult = await client.query(
                `INSERT INTO game_stats (user_id, wins, losses, kills, deaths, games_played, playtime_seconds, accuracy, headshots)
                 VALUES ($1,
                     CASE WHEN $2 THEN 1 ELSE 0 END,
                     CASE WHEN $2 THEN 0 ELSE 1 END,
                     $3, $4, 1, $5, $6, $7)
                 ON CONFLICT (user_id) DO UPDATE SET
                     wins = game_stats.wins + CASE WHEN $2 THEN 1 ELSE 0 END,
                     losses = game_stats.losses + CASE WHEN $2 THEN 0 ELSE 1 END,
                     kills = game_stats.kills + $3,
                     deaths = game_stats.deaths + $4,
                     games_played = game_stats.games_played + 1,
                     playtime_seconds = game_stats.playtime_seconds + $5,
                     headshots = game_stats.headshots + $7,
                     accuracy = (game_stats.accuracy * game_stats.games_played + $6) / (game_stats.games_played + 1),
                     highest_mmr = GREATEST(game_stats.highest_mmr, game_stats.current_mmr),
                     updated_at = NOW()
                 RETURNING *`,
                [req.user.id, won, kills || 0, deaths || 0,
                 playtime_seconds || 0, accuracy || 0, headshots || 0]
            )

            // Record match if match_id provided
            if (match_id) {
                await client.query(
                    `INSERT INTO match_history (match_id, map_name, game_mode, duration_seconds, winner_id)
                     VALUES ($1, $2, $3, $4, CASE WHEN $5 THEN $6 ELSE NULL END)
                     ON CONFLICT (match_id) DO NOTHING`,
                    [match_id, map_name || '', game_mode || '', playtime_seconds || 0, won, req.user.id]
                )

                await client.query(
                    `INSERT INTO match_participants (match_id, user_id, username, kills, deaths, accuracy, headshots, damage_dealt, won, mmr_before, mmr_after)
                     VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11)
                     ON CONFLICT (match_id, user_id) DO NOTHING`,
                    [match_id, req.user.id, req.user.username,
                     kills || 0, deaths || 0, accuracy || 0,
                     headshots || 0, damage_dealt || 0, won,
                     req.body.mmr_before || 5000,
                     statsResult.rows[0]?.current_mmr || 5000]
                )
            }

            await client.query("COMMIT")
            logAuth("stats_update", `user_id=${req.user.id}`)
            res.json({ success: true, stats: statsResult.rows[0] })
        }
        catch (error) {
            await client.query("ROLLBACK")
            throw error
        }
        finally {
            client.release()
        }
    }
    catch (error) {
        next(error)
    }
})

// ── Leaderboard ──────────────────────────────────────────────────────────────

app.get("/api/leaderboard", async (req, res, next) => {
    try {
        const type = String(req.query.type || "mmr")
        const limit = Math.min(100, Math.max(1, Number(req.query.limit) || 50))
        const period = String(req.query.period || "all") // all, daily, weekly, monthly

        let orderBy = "current_mmr DESC"
        let selectExtra = ""
        let joinExtra = ""

        switch (type) {
            case "wins":
                orderBy = "gs.wins DESC"
                break
            case "playtime":
                orderBy = "gs.playtime_seconds DESC"
                break
            case "accuracy":
                orderBy = "gs.accuracy DESC"
                break
            case "killstreak":
                orderBy = "gs.best_kill_streak DESC"
                break
            case "kills":
                orderBy = "gs.kills DESC"
                break
            case "daily":
                orderBy = "gs.current_mmr DESC"
                joinExtra = "AND u.last_login_at >= CURRENT_DATE"
                break
            case "weekly":
                orderBy = "gs.current_mmr DESC"
                joinExtra = "AND u.last_login_at >= CURRENT_DATE - INTERVAL '7 days'"
                break
            case "monthly":
                orderBy = "gs.current_mmr DESC"
                joinExtra = "AND u.last_login_at >= CURRENT_DATE - INTERVAL '30 days'"
                break
            default:
                orderBy = "gs.current_mmr DESC"
                break
        }

        const result = await pool.query(
            `SELECT u.id, u.username, u.avatar_url, u.avatar_updated_at, u.supporter_tier,
                    gs.wins, gs.losses, gs.kills, gs.deaths, gs.games_played,
                    gs.playtime_seconds, gs.current_mmr, gs.highest_mmr,
                    gs.accuracy, gs.headshots, gs.best_kill_streak
             FROM game_stats gs
             JOIN users u ON u.id = gs.user_id
             WHERE u.deleted_at IS NULL ${joinExtra}
             ORDER BY ${orderBy}
             LIMIT $1`,
            [limit]
        )

        res.json({
            success: true,
            leaderboard: result.rows.map((row, idx) => ({
                rank: idx + 1,
                ...row
            })),
            type,
            limit
        })
    }
    catch (error) {
        next(error)
    }
})

// ── Match History ────────────────────────────────────────────────────────────

app.get("/api/match-history", authenticate, async (req, res, next) => {
    try {
        const page = Math.max(1, Number(req.query.page) || 1)
        const limit = Math.min(50, Math.max(1, Number(req.query.limit) || 20))
        const offset = (page - 1) * limit

        const countResult = await pool.query(
            `SELECT COUNT(*) AS count FROM match_participants WHERE user_id = $1`,
            [req.user.id]
        )
        const total = Number(countResult.rows[0].count)

        const result = await pool.query(
            `SELECT mh.match_id, mh.map_name, mh.game_mode, mh.duration_seconds,
                    mh.created_at, mp.kills, mp.deaths, mp.accuracy, mp.headshots,
                    mp.damage_dealt, mp.won, mp.mmr_before, mp.mmr_after
             FROM match_participants mp
             JOIN match_history mh ON mh.match_id = mp.match_id
             WHERE mp.user_id = $1
             ORDER BY mh.created_at DESC
             LIMIT $2 OFFSET $3`,
            [req.user.id, limit, offset]
        )

        res.json({
            success: true,
            matches: result.rows,
            total,
            page,
            limit,
            pages: Math.ceil(total / limit)
        })
    }
    catch (error) {
        next(error)
    }
})

// ── User Settings (Cloud Sync) ───────────────────────────────────────────────

app.get("/api/settings", authenticate, async (req, res, next) => {
    try {
        const result = await pool.query(
            `SELECT settings_json FROM user_settings WHERE user_id = $1`,
            [req.user.id]
        )
        res.json({
            success: true,
            settings: result.rows[0]?.settings_json || {}
        })
    }
    catch (error) {
        next(error)
    }
})

app.put("/api/settings", authenticate, async (req, res, next) => {
    try {
        const settingsJson = req.body.settings || {}

        await pool.query(
            `INSERT INTO user_settings (user_id, settings_json, updated_at)
             VALUES ($1, $2, NOW())
             ON CONFLICT (user_id) DO UPDATE SET
                 settings_json = $2,
                 updated_at = NOW()`,
            [req.user.id, JSON.stringify(settingsJson)]
        )

        logAuth("settings_update", `user_id=${req.user.id}`)
        res.json({ success: true, message: "settings saved" })
    }
    catch (error) {
        next(error)
    }
})

// ── Token Exchange (mimita:// flow) ──────────────────────────────────────────

const exchangeTokens = new Map()

app.post("/api/auth/token-exchange", authRateLimit, async (req, res, next) => {
    try {
        // Authenticated user on website generates a one-time exchange token
        const token = parseCookies(req)[sessionCookieName]
        if (!token) {
            return res.status(401).json({ success: false, message: "sign in required" })
        }

        const sessionResult = await pool.query(
            `SELECT u.id, u.username, u.avatar_url, u.supporter_tier, u.role
             FROM sessions s JOIN users u ON u.id = s.user_id
             WHERE s.token_hash = $1 AND s.revoked_at IS NULL AND s.expires_at > NOW() AND u.deleted_at IS NULL
             LIMIT 1`,
            [hashToken(token, sessionSecret)]
        )
        if (!sessionResult.rowCount) {
            return res.status(401).json({ success: false, message: "session invalid" })
        }

        const user = sessionResult.rows[0]
        const exchangeToken = crypto.randomBytes(32).toString("hex")
        exchangeTokens.set(exchangeToken, {
            userId: user.id,
            username: user.username,
            avatarUrl: user.avatar_url,
            supporterTier: user.supporter_tier,
            role: user.role,
            createdAt: Date.now()
        })
        setTimeout(() => exchangeTokens.delete(exchangeToken), 60 * 1000) // 1 min expiry

        // Update last login
        await pool.query(`UPDATE users SET last_login_at = NOW() WHERE id = $1`, [user.id])

        res.json({
            success: true,
            exchange_token: exchangeToken,
            expires_in: 60
        })
    }
    catch (error) {
        next(error)
    }
})

app.post("/api/auth/exchange-session", authRateLimit, async (req, res, next) => {
    try {
        const exchangeToken = String(req.body.exchange_token || "").trim()
        const entry = exchangeTokens.get(exchangeToken)

        if (!entry) {
            return res.status(404).json({ success: false, message: "invalid or expired exchange token" })
        }

        exchangeTokens.delete(exchangeToken)

        // Create a game session for this user
        const sessionToken = createSecretToken()
        const tokenHash = hashToken(sessionToken, sessionSecret)
        await pool.query(
            `INSERT INTO sessions (user_id, token_hash, user_agent, ip_address, expires_at)
             VALUES ($1, $2, $3, $4, NOW() + ($5 * INTERVAL '1 day'))`,
            [entry.userId, tokenHash, "game-client", getClientIp(req), sessionDays]
        )

        // Get full profile data
        const profileResult = await pool.query(
            `SELECT id, username, email, bio, avatar_url, avatar_updated_at, avatar_data,
                    supporter_tier, role, achievements, email_verified_at IS NOT NULL AS email_verified,
                    created_at, display_name, last_login_at
             FROM users WHERE id = $1`,
            [entry.userId]
        )
        const profile = profileResult.rows[0]

        const statsResult = await pool.query(
            `SELECT * FROM game_stats WHERE user_id = $1`,
            [entry.userId]
        )

        res.json({
            success: true,
            session_token: sessionToken,
            user: {
                id: profile.id,
                username: profile.username,
                display_name: profile.display_name || profile.username,
                email: profile.email,
                bio: profile.bio,
                avatar_url: profile.avatar_url,
                avatar_updated_at: profile.avatar_updated_at,
                avatar_data: profile.avatar_data,
                supporter_tier: profile.supporter_tier,
                role: profile.role,
                achievements: profile.achievements,
                email_verified: profile.email_verified,
                created_at: profile.created_at
            },
            stats: statsResult.rows[0] || null
        })
    }
    catch (error) {
        next(error)
    }
})

// ── Desktop Detection ────────────────────────────────────────────────────────

app.get("/api/desktop/detect", (req, res) => {
    // The frontend checks if the mimita:// protocol is registered
    // by attempting to create a hidden iframe or checking navigator
    res.json({
        success: true,
        protocol: "mimita://",
        launcher_path: "MimitaLauncher.exe",
        description: "Open Mimita?"
    })
})

// ── Client Login Codes (4-letter) ────────────────────────────────────────────

const clientLoginRateLimit = createRateLimit({ windowMs: 60 * 1000, max: 10, name: "client_login" })
const clientLoginPreviewRateLimit = createRateLimit({ windowMs: 60 * 1000, max: 30, name: "client_login_preview" })

function generateClientCode() {
    const chars = "ABCDEFGHJKLMNPQRSTUVWXYZ"
    let code = ""
    for (let i = 0; i < 4; i++) {
        code += chars[crypto.randomInt(0, chars.length)]
    }
    return code
}

app.post("/api/client-login/create-code", authRateLimit, async (req, res, next) => {
    try {
        const token = parseCookies(req)[sessionCookieName]
        if (!token) {
            return res.status(401).json({ success: false, message: "sign in required" })
        }

        const sessionResult = await pool.query(
            `SELECT u.id, u.username, u.avatar_url, u.avatar_data, u.display_name, u.supporter_tier
             FROM sessions s JOIN users u ON u.id = s.user_id
             WHERE s.token_hash = $1 AND s.revoked_at IS NULL AND s.expires_at > NOW() AND u.deleted_at IS NULL
             LIMIT 1`,
            [hashToken(token, sessionSecret)]
        )
        if (!sessionResult.rowCount) {
            return res.status(401).json({ success: false, message: "session invalid" })
        }

        // Invalidate any previous unused codes for this user
        await pool.query(
            `UPDATE client_login_codes SET used_at = NOW()
             WHERE user_id = $1 AND used_at IS NULL AND expires_at > NOW()`,
            [sessionResult.rows[0].id]
        )

        const code = generateClientCode()
        const codeHash = hashToken(code, sessionSecret)
        const expiresAt = new Date(Date.now() + 5 * 60 * 1000)

        await pool.query(
            `INSERT INTO client_login_codes (user_id, code_hash, expires_at, ip_address, user_agent)
             VALUES ($1, $2, $3, $4, $5)`,
            [sessionResult.rows[0].id, codeHash, expiresAt, getClientIp(req), req.get("user-agent") || "unknown"]
        )

        logAuth("client_code_create", `user_id=${sessionResult.rows[0].id}`)
        res.json({
            success: true,
            code,
            expires_at: expiresAt.toISOString()
        })
    }
    catch (error) {
        next(error)
    }
})

app.post("/api/client-login/preview", clientLoginPreviewRateLimit, async (req, res, next) => {
    try {
        const rawCode = String(req.body.code || "").trim().toUpperCase()
        if (!/^[A-Z]{4}$/.test(rawCode)) {
            return res.status(400).json({ success: false, valid: false, message: "invalid code format" })
        }

        const codeHash = hashToken(rawCode, sessionSecret)
        const result = await pool.query(
            `SELECT clc.user_id, u.username, u.display_name, u.avatar_url, u.avatar_data,
                    u.supporter_tier, u.achievements
             FROM client_login_codes clc
             JOIN users u ON u.id = clc.user_id
             WHERE clc.code_hash = $1
               AND clc.used_at IS NULL
               AND clc.expires_at > NOW()
             LIMIT 1`,
            [codeHash]
        )

        if (!result.rowCount) {
            return res.json({ success: true, valid: false, message: "invalid or expired code" })
        }

        const user = result.rows[0]
        res.json({
            success: true,
            valid: true,
            username: user.username,
            display_name: user.display_name || user.username,
            avatar_url: user.avatar_url,
            avatar_data: user.avatar_data,
            supporter_tier: user.supporter_tier,
            achievements: user.achievements
        })
    }
    catch (error) {
        next(error)
    }
})

app.post("/api/client-login/confirm", clientLoginRateLimit, async (req, res, next) => {
    try {
        const rawCode = String(req.body.code || "").trim().toUpperCase()
        if (!/^[A-Z]{4}$/.test(rawCode)) {
            return res.status(400).json({ success: false, message: "invalid code format" })
        }

        const codeHash = hashToken(rawCode, sessionSecret)
        const client = await pool.connect()

        try {
            await client.query("BEGIN")

            const codeResult = await client.query(
                `SELECT clc.id, clc.user_id
                 FROM client_login_codes clc
                 WHERE clc.code_hash = $1
                   AND clc.used_at IS NULL
                   AND clc.expires_at > NOW()
                 LIMIT 1
                 FOR UPDATE`,
                [codeHash]
            )

            if (!codeResult.rowCount) {
                await client.query("ROLLBACK")
                return res.json({ success: false, message: "invalid or expired code" })
            }

            const userId = codeResult.rows[0].user_id

            // Mark code as used
            await client.query(
                `UPDATE client_login_codes SET used_at = NOW() WHERE id = $1`,
                [codeResult.rows[0].id]
            )

            // Create game session
            const sessionToken = createSecretToken()
            const tokenHash = hashToken(sessionToken, sessionSecret)
            await client.query(
                `INSERT INTO sessions (user_id, token_hash, user_agent, ip_address, expires_at)
                 VALUES ($1, $2, $3, $4, NOW() + ($5 * INTERVAL '1 day'))`,
                [userId, tokenHash, "game-client", getClientIp(req), sessionDays]
            )

            // Update last login
            await client.query(`UPDATE users SET last_login_at = NOW() WHERE id = $1`, [userId])

            // Fetch full profile
            const profileResult = await client.query(
                `SELECT id, username, email, bio, avatar_url, avatar_updated_at, avatar_data,
                        display_name, supporter_tier, role, achievements,
                        email_verified_at IS NOT NULL AS email_verified,
                        created_at
                 FROM users WHERE id = $1`,
                [userId]
            )
            const profile = profileResult.rows[0]

            const statsResult = await client.query(
                `SELECT * FROM game_stats WHERE user_id = $1`,
                [userId]
            )

            await client.query("COMMIT")

            logAuth("client_login_confirm", `user_id=${userId}`)
            res.json({
                success: true,
                session_token: sessionToken,
                account: {
                    id: profile.id,
                    username: profile.username
                },
                profile: {
                    id: profile.id,
                    username: profile.username,
                    display_name: profile.display_name || profile.username,
                    email: profile.email,
                    bio: profile.bio,
                    avatar_url: profile.avatar_url,
                    avatar_updated_at: profile.avatar_updated_at,
                    avatar_data: profile.avatar_data,
                    supporter_tier: profile.supporter_tier,
                    role: profile.role,
                    achievements: profile.achievements,
                    email_verified: profile.email_verified,
                    created_at: profile.created_at
                },
                avatar: {
                    url: profile.avatar_url,
                    data: profile.avatar_data,
                    updated_at: profile.avatar_updated_at
                },
                stats: statsResult.rows[0] || null
            })
        }
        catch (error) {
            await client.query("ROLLBACK")
            throw error
        }
        finally {
            client.release()
        }
    }
    catch (error) {
        next(error)
    }
})

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
        res.json({ success: true, code, grantToken, url: "https://mimita.fun/link" })
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

app.use("/api/download/file", (req, res, next) => {
    if (req.method !== "GET") return next()
    const safePath = String(req.path.replace("/api/download/file/", "").replace(/^\/+/, "") || "")
    const filePath = path.resolve(safePath)
    const resolved = path.resolve(filePath)
    const gameDir = path.resolve(".")
    if (!resolved.startsWith(gameDir)) {
        return res.status(403).json({ success: false, message: "forbidden" })
    }
    if (fs.existsSync(resolved)) {
        return res.sendFile(resolved)
    }
    const rootPath = path.resolve("..", safePath)
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
