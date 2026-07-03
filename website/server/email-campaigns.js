import { Router } from "express"
import { pool } from "./db.js"
import { getClientIp } from "./authCore.js"
import { sendMail } from "./mail.js"
import { requireAdmin } from "./admin.js"

const router = Router()

// GET /api/admin/email-campaigns/users — list all users
router.get("/users", requireAdmin, async (req, res, next) => {
    try {
        const search = req.query.search || ""
        let sql = `
            SELECT id, username, email, role, created_at, avatar_url,
                   email_verified_at IS NOT NULL AS email_verified,
                   email_notifications_enabled
            FROM users
            WHERE deleted_at IS NULL
        `
        const params = []
        if (search) {
            sql += ` AND (username ILIKE $1 OR email ILIKE $1 OR role ILIKE $1)`
            params.push(`%${search}%`)
        }
        sql += ` ORDER BY created_at DESC`
        const result = await pool.query(sql, params)
        console.log(`[EMAIL CAMPAIGN] Loaded ${result.rowCount} users search="${search}"`)
        res.json({ success: true, users: result.rows, total: result.rowCount })
    } catch (error) { next(error) }
})

// GET /api/admin/email-campaigns/templates — list saved templates
router.get("/templates", requireAdmin, async (req, res, next) => {
    try {
        const result = await pool.query(`
            SELECT id, name, description, html_body, created_by, created_at, updated_at
            FROM email_templates
            ORDER BY updated_at DESC
        `)
        console.log(`[EMAIL CAMPAIGN] Loaded ${result.rowCount} templates`)
        res.json({ success: true, templates: result.rows })
    } catch (error) { next(error) }
})

// POST /api/admin/email-campaigns/templates — save a template
router.post("/templates", requireAdmin, async (req, res, next) => {
    try {
        const { name, description, htmlBody } = req.body
        if (!name || !htmlBody) return res.status(400).json({ success: false, message: "name and htmlBody required" })
        const result = await pool.query(`
            INSERT INTO email_templates (name, description, html_body, created_by)
            VALUES ($1, $2, $3, $4)
            RETURNING id, name, description, html_body, created_by, created_at
        `, [name, description || "", htmlBody, req.user.username])
        console.log(`[EMAIL CAMPAIGN] Template saved id=${result.rows[0].id} name="${name}"`)
        res.json({ success: true, template: result.rows[0] })
    } catch (error) { next(error) }
})

// POST /api/admin/email-campaigns/send — send a campaign
router.post("/send", requireAdmin, async (req, res, next) => {
    try {
        const { subject, htmlBody, recipientIds, templateId } = req.body
        if (!subject || !htmlBody || !recipientIds?.length)
            return res.status(400).json({ success: false, message: "subject, htmlBody, and recipientIds required" })

        console.log(`[EMAIL CAMPAIGN] Starting campaign subject="${subject}" recipients=${recipientIds.length}`)

        // Get user emails
        const users = await pool.query(`
            SELECT id, username, email, email_verified_at IS NOT NULL AS email_verified
            FROM users WHERE id = ANY($1::bigint[]) AND deleted_at IS NULL
        `, [recipientIds])
        console.log(`[EMAIL CAMPAIGN] Found ${users.rowCount} valid users from ${recipientIds.length} IDs`)

        // Create campaign
        const campaign = await pool.query(`
            INSERT INTO email_campaigns (subject, html_body, template_id, status, total_recipients, created_by)
            VALUES ($1, $2, $3, 'sending', $4, $5)
            RETURNING id
        `, [subject, htmlBody, templateId || null, users.rowCount, req.user.username])
        const campaignId = campaign.rows[0].id
        console.log(`[EMAIL CAMPAIGN] Campaign created id=${campaignId}`)

        // Insert recipients
        let inserted = 0
        for (const user of users.rows) {
            await pool.query(`
                INSERT INTO email_campaign_recipients (campaign_id, user_id, email, username, status)
                VALUES ($1, $2, $3, $4, 'pending')
            `, [campaignId, user.id, user.email, user.username])
            inserted++
        }
        console.log(`[EMAIL CAMPAIGN] Inserted ${inserted} recipients`)

        // Send emails with progress
        let delivered = 0, failed = 0, skipped = 0, rejected = 0
        let invalidEmail = 0, smtpError = 0, dbError = 0

        for (let i = 0; i < users.rows.length; i++) {
            const user = users.rows[i]
            const recipientStatus = async (status, errorMsg) => {
                try {
                    await pool.query(`
                        UPDATE email_campaign_recipients
                        SET status = $1, error_message = $2, sent_at = CASE WHEN $1 = 'delivered' THEN NOW() ELSE sent_at END
                        WHERE campaign_id = $3 AND user_id = $4
                    `, [status, errorMsg || "", campaignId, user.id])
                } catch (e) {
                    console.log(`[EMAIL CAMPAIGN] DB error updating recipient ${user.id}: ${e.message}`)
                }
            }

            try {
                await recipientStatus("sending", "")

                if (!user.email || !user.email.includes("@")) {
                    await recipientStatus("invalid_email", "no valid email")
                    invalidEmail++
                    console.log(`[EMAIL CAMPAIGN] Skipped ${user.username} (${user.email}): invalid email`)
                    continue
                }

                if (!user.email_verified) {
                    await recipientStatus("skipped", "email not verified")
                    skipped++
                    console.log(`[EMAIL CAMPAIGN] Skipped ${user.username} (${user.email}): not verified`)
                    continue
                }

                const info = await sendMail({
                    to: `"${user.username}" <${user.email}>`,
                    subject: subject,
                    html: htmlBody
                })

                if (info && info.accepted && info.accepted.length > 0) {
                    await recipientStatus("delivered", "")
                    delivered++
                    console.log(`[EMAIL CAMPAIGN] Delivered ${i + 1}/${users.rows.length} to ${user.email} id=${info.messageId || "unknown"}`)
                } else if (info && info.rejected && info.rejected.length > 0) {
                    await recipientStatus("rejected", `SMTP rejected: ${info.rejected.join(",")}`)
                    rejected++
                    console.log(`[EMAIL CAMPAIGN] Rejected ${user.email}: ${info.rejected.join(",")}`)
                } else {
                    await recipientStatus("failed", "unknown send result")
                    failed++
                    console.log(`[EMAIL CAMPAIGN] Failed ${user.email}: unknown result`)
                }
            } catch (error) {
                const errMsg = error.message || "unknown error"
                if (errMsg.includes("Invalid email") || errMsg.includes("bad address")) {
                    await recipientStatus("invalid_email", errMsg)
                    invalidEmail++
                } else if (errMsg.includes("connect") || errMsg.includes("ETIMEOUT") || errMsg.includes("ECONN")) {
                    await recipientStatus("smtp_error", errMsg)
                    smtpError++
                } else {
                    await recipientStatus("failed", errMsg)
                    failed++
                }
                console.log(`[EMAIL CAMPAIGN] Error ${user.email}: ${errMsg}`)
            }
        }

        // Update campaign totals
        await pool.query(`
            UPDATE email_campaigns SET
                status = CASE WHEN $1 = 0 AND $2 = 0 AND $3 = 0 THEN 'sent' WHEN $4 > 0 THEN 'partial' ELSE 'failed' END,
                delivered_count = $1,
                failed_count = $2,
                skipped_count = $3,
                rejected_count = $5,
                invalid_email_count = $6,
                smtp_error_count = $7,
                db_error_count = $8,
                sent_at = NOW(),
                updated_at = NOW()
            WHERE id = $9
        `, [delivered, failed, skipped, failed + rejected, rejected, invalidEmail, smtpError, dbError, campaignId])

        const result = { delivered, failed, skipped, rejected, invalidEmail, smtpError, dbError, campaignId }
        console.log(`[EMAIL CAMPAIGN] Complete id=${campaignId}`, JSON.stringify(result))
        res.json({ success: true, ...result })
    } catch (error) { next(error) }
})

// GET /api/admin/email-campaigns/list — list past campaigns
router.get("/list", requireAdmin, async (req, res, next) => {
    try {
        const result = await pool.query(`
            SELECT id, subject, status, total_recipients, delivered_count, failed_count,
                   skipped_count, rejected_count, invalid_email_count, smtp_error_count,
                   created_by, created_at, sent_at
            FROM email_campaigns
            ORDER BY created_at DESC
            LIMIT 50
        `)
        console.log(`[EMAIL CAMPAIGN] Listed ${result.rowCount} campaigns`)
        res.json({ success: true, campaigns: result.rows })
    } catch (error) { next(error) }
})

// GET /api/admin/email-campaigns/:id — campaign details with recipients
router.get("/:id", requireAdmin, async (req, res, next) => {
    try {
        const campaign = await pool.query(`
            SELECT * FROM email_campaigns WHERE id = $1
        `, [req.params.id])
        if (!campaign.rowCount) return res.status(404).json({ success: false, message: "campaign not found" })

        const recipients = await pool.query(`
            SELECT id, user_id, email, username, status, error_message, sent_at
            FROM email_campaign_recipients
            WHERE campaign_id = $1
            ORDER BY id
        `, [req.params.id])

        console.log(`[EMAIL CAMPAIGN] Detail id=${req.params.id} recipients=${recipients.rowCount}`)
        res.json({ success: true, campaign: campaign.rows[0], recipients: recipients.rows })
    } catch (error) { next(error) }
})

export default router
