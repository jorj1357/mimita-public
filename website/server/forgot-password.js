// 08 02 2026, 16 00
/* purpose
* Forgot-password reset flow for the website: request a reset code by identifier
* and reset the password with a verified code.
* Covers unauthenticated users who cannot use the password-change flow.
* DOES NOT handle signed-in password changes (see server.js).
* DOES NOT send email; callers inject sendResetCodeEmail / sendPasswordChangedEmail.
*/

import express from "express"

import {
    createSixDigitCode,
    getClientIp,
    hashPassword,
    hashToken,
    normalizeEmail,
    usernameKey,
    validatePassword
} from "./authCore.js"

export function createForgotPasswordRouter(deps) {
    const {
        pool,
        rateLimit,
        sessionSecret,
        logAuth,
        sendResetCodeEmail,
        sendPasswordChangedEmail
    } = deps

    const router = express.Router()

    router.post("/request", rateLimit, async (req, res, next) => {
        try {
            const identifier = String(req.body.identifier || "").trim()

            if (!identifier) {
                return res.status(400).json({
                    success: false,
                    message: "enter your username or email"
                })
            }

            const result = await pool.query(
                `
                SELECT id, email
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

            if (user) {
                const code = createSixDigitCode()
                const codeHash = hashToken(code, sessionSecret)

                await pool.query(
                    `
                    UPDATE password_reset_codes
                    SET used_at = NOW()
                    WHERE user_id = $1
                      AND used_at IS NULL
                    `,
                    [user.id]
                )

                await pool.query(
                    `
                    INSERT INTO password_reset_codes (
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
                        user.id,
                        codeHash,
                        getClientIp(req),
                        req.get("user-agent") || "unknown"
                    ]
                )

                await sendResetCodeEmail(user.email, code)
                logAuth("forgot_password", `request_sent user_id=${user.id}`)
            }

            res.json({
                success: true,
                message: "if an account exists, a reset code was sent"
            })
        }
        catch (error) {
            logAuth("forgot_password", "request_failed")
            next(error)
        }
    })

    router.post("/reset", rateLimit, async (req, res, next) => {
        const client = await pool.connect()

        try {
            const identifier = String(req.body.identifier || "").trim()
            const code = String(req.body.code || "").trim()
            const newPassword = String(req.body.newPassword || "")
            const confirmPassword = String(req.body.confirmNewPassword || "")

            if (!/^\d{6}$/.test(code)) {
                return res.status(400).json({
                    success: false,
                    message: "enter the 6-digit code"
                })
            }

            const passwordValidation = validatePassword(newPassword)
            if (!passwordValidation.ok) {
                return res.status(400).json({
                    success: false,
                    message: passwordValidation.message
                })
            }

            if (newPassword !== confirmPassword) {
                return res.status(400).json({
                    success: false,
                    message: "new passwords do not match"
                })
            }

            const userResult = await client.query(
                `
                SELECT id, email, email_notifications_enabled
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
            const user = userResult.rows[0]

            if (!user) {
                logAuth("forgot_password", "reset_unknown_identifier")
                return res.status(400).json({
                    success: false,
                    message: "invalid or expired code"
                })
            }

            await client.query("BEGIN")

            const codeResult = await client.query(
                `
                SELECT id
                FROM password_reset_codes
                WHERE user_id = $1
                  AND code_hash = $2
                  AND used_at IS NULL
                  AND expires_at > NOW()
                ORDER BY created_at DESC
                LIMIT 1
                FOR UPDATE
                `,
                [user.id, hashToken(code, sessionSecret)]
            )

            if (!codeResult.rowCount) {
                await client.query("ROLLBACK")
                logAuth("forgot_password", "invalid_code")
                return res.status(400).json({
                    success: false,
                    message: "invalid or expired code"
                })
            }

            const newHash = await hashPassword(newPassword)
            await client.query(
                `
                UPDATE users
                SET password_hash = $1, updated_at = NOW()
                WHERE id = $2
                `,
                [newHash, user.id]
            )
            await client.query(
                `
                UPDATE password_reset_codes
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
                  AND revoked_at IS NULL
                `,
                [user.id]
            )
            await client.query("COMMIT")

            logAuth("forgot_password", `reset_success user_id=${user.id}`)

            if (user.email_notifications_enabled) {
                await sendPasswordChangedEmail(
                    user.email,
                    new Date().toISOString(),
                    req.get("user-agent") || "unknown",
                    getClientIp(req)
                )
            }

            res.json({
                success: true,
                message: "password reset"
            })
        }
        catch (error) {
            await client.query("ROLLBACK").catch(() => {})
            logAuth("forgot_password", "reset_failed")
            next(error)
        }
        finally {
            client.release()
        }
    })

    return router
}
