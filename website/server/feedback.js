import { pool } from "./db.js"
import { trackEvent } from "./analytics.js"

export const FEEDBACK_PRESETS = [
    "Cool Site",
    "Bad Site",
    "Found A Bug",
    "Confusing Layout",
    "What's The Point?",
    "I Like This",
    "I Don't Like This"
]

export async function submitFeedback({ selectedPresets, customFeedback, contactInfo, pageUrl, userId }) {
    await trackEvent("feedback_submit", {
        event_data: {
            has_presets: Boolean(selectedPresets?.length),
            has_custom: Boolean(customFeedback)
        },
        user_id: userId || null,
        page_url: pageUrl || null
    })

    const result = await pool.query(
        `
        INSERT INTO feedback (selected_presets, custom_feedback, contact_info, page_url, user_id)
        VALUES ($1, $2, $3, $4, $5)
        RETURNING id, created_at
        `,
        [
            selectedPresets || [],
            (customFeedback || "").slice(0, 200),
            contactInfo || "",
            pageUrl || "",
            userId || null
        ]
    )
    return result.rows[0]
}

export async function getFeedback(limit = 20, offset = 0) {
    const result = await pool.query(
        `
        SELECT f.id, f.selected_presets, f.custom_feedback, f.contact_info,
               f.page_url, f.user_id, f.status, f.category, f.created_at,
               u.username, u.avatar_url
        FROM feedback f
        LEFT JOIN users u ON u.id = f.user_id
        ORDER BY f.created_at DESC
        LIMIT $1 OFFSET $2
        `,
        [limit, offset]
    )
    return result.rows
}

export async function getFeedbackCount() {
    const result = await pool.query(`SELECT COUNT(*) AS count FROM feedback`)
    return Number(result.rows[0].count)
}

export async function updateFeedbackStatus(id, status) {
    const valid = ["new", "reviewed", "completed", "ignored"]
    if (!valid.includes(status)) {
        throw new Error(`invalid status: ${status}`)
    }
    await pool.query(
        `UPDATE feedback SET status = $1 WHERE id = $2`,
        [status, id]
    )
}
