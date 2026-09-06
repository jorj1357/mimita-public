// 09 06 2026, 16 00
/* purpose
 * Persist authenticated community hosts' cumulative session progression.
 * Bind remote identities to join tickets and acknowledge only committed batches.
 * Preserve concurrent host gains using transactional additive updates.
 * DOES NOT validate gameplay events or decide reward amounts.
 * DOES NOT accept client writes outside registered host membership.
 * DOES NOT log credentials or call production services.
 */
import { Router } from "express"
import { createHash } from "node:crypto"
import { hashToken } from "./authCore.js"
import { sessionSecret } from "./session.js"
import { createRateLimit } from "./rateLimit.js"

export const progressionFields = {
    gold: "gold", totalXp: "total_xp", playerKills: "lifetime_player_kills",
    npcKills: "lifetime_npc_kills", deaths: "lifetime_deaths", playtimeTicks: "playtime_ticks"
}
const columns = Object.values(progressionFields)
const maximum = 9223372036854775807n

function reject(code, status = 400) {
    throw Object.assign(new Error(code), { status, progressionCode: code })
}

export function progressionInteger(value, positive = false) {
    if (typeof value === "number" && (!Number.isSafeInteger(value) || value < 0)) reject("invalid_integer")
    if (!["number", "string"].includes(typeof value) || !/^(0|[1-9][0-9]{0,18})$/.test(String(value))) reject("invalid_integer")
    const integer = BigInt(value)
    if (integer > maximum || (positive && integer === 0n)) reject("invalid_integer")
    return integer.toString()
}

function stableId(value) {
    if (typeof value !== "string" || !/^[A-Za-z0-9_-]{1,128}$/.test(value)) reject("invalid_id")
    return value
}

export function progressionTotals(row = {}) {
    return Object.fromEntries(Object.entries(progressionFields).map(([key, column]) => [key, String(row[column] ?? 0)]))
}

export function createProgressionRouter({ database, authenticateMw, log = console.info }) {
    const router = Router()
    const lastLogs = new Map()
    const diagnose = fields => {
        const key = `${fields.operation}:${fields.decision}:${fields.reason || ""}`
        const now = Date.now()
        if (now - (lastLogs.get(key) || 0) < 1000) return
        lastLogs.set(key, now)
        log("[Progression]", JSON.stringify(fields))
    }
    // Host credentials must be explicitly supplied; browser cookies cannot authorize writes.
    router.use((req, res, next) => {
        if (!/^Bearer \S+$/.test(req.headers.authorization || "")) {
            return res.status(401).json({ success: false, error: "host_bearer_required" })
        }
        next()
    }, (req, res, next) => authenticateMw(req, res, error => {
        if (error) return res.status(503).json({ success: false, error: "host_auth_unavailable" })
        next()
    }), createRateLimit({ name: "progression", max: 120 }))

    for (const operation of ["session", "batch"]) {
        router.post(`/${operation}`, async (req, res) => {
            let client
            let sessionId, batchId, hostId
            try {
                const body = req.body || {}
                sessionId = stableId(body.sessionId)
                hostId = progressionInteger(req.user.id, true)
                if (!Array.isArray(body.players) || body.players.length > 128) reject("invalid_players")
                batchId = operation === "batch" ? stableId(body.batchId) : null
                const reason = body.reason ?? ""
                if (typeof reason !== "string" || reason.length > 64) reject("invalid_reason")
                if (operation === "batch" && body.players.length === 0) reject("empty_batch")
                const roomCode = body.roomCode
                if (operation === "session" && (typeof roomCode !== "string" || roomCode.length > 32 || roomCode !== roomCode.trim())) reject("invalid_room")
                client = await database.connect()
                await client.query("BEGIN")
                if (operation === "session") {
                    await client.query(`INSERT INTO progression_sessions(session_id, host_user_id, room_code)
                        VALUES ($1,$2,$3) ON CONFLICT DO NOTHING`, [sessionId, hostId, roomCode])
                }
                // One lock serializes session registration, duplicate batches and acknowledgements.
                const session = (await client.query("SELECT * FROM progression_sessions WHERE session_id=$1 FOR UPDATE", [sessionId])).rows[0]
                if (!session || String(session.host_user_id) !== hostId) reject("host_session_forbidden", 403)
                if (operation === "session" && session.room_code !== roomCode) reject("session_room_mismatch", 409)
                let response
                if (operation === "session") {
                    const players = []
                    const seen = new Set()
                    const sortedPlayers = body.players.map(player => {
                        if (!player || typeof player !== "object") reject("invalid_player")
                        return { ...player, userId: progressionInteger(player.userId, true) }
                    }).sort((a, b) => BigInt(a.userId) < BigInt(b.userId) ? -1 : 1)
                    for (const player of sortedPlayers) {
                        const userId = player.userId
                        if (seen.has(userId)) reject("duplicate_player")
                        seen.add(userId)
                        let membership = (await client.query(`SELECT * FROM progression_players
                            WHERE session_id=$1 AND user_id=$2`, [sessionId, userId])).rows[0]
                        if (!membership) {
                            let ticketHash = null
                            if (userId !== hostId) {
                                if (!roomCode || typeof player.joinTicket !== "string" || player.joinTicket.length > 1024 || !player.joinTicket) reject("join_ticket_required", 403)
                                ticketHash = hashToken(player.joinTicket, sessionSecret)
                                const ticket = (await client.query(`SELECT * FROM vip_join_tickets
                                    WHERE token_hash=$1 AND expires_at > NOW() FOR UPDATE`, [ticketHash])).rows[0]
                                if (!ticket || String(ticket.user_id) !== userId || ticket.room_code !== roomCode
                                    || (ticket.used_at && String(ticket.last_result?.account_id) !== userId)) reject("join_ticket_invalid", 403)
                                const bound = await client.query("SELECT 1 FROM progression_players WHERE ticket_hash=$1", [ticketHash])
                                if (bound.rowCount) reject("join_ticket_already_bound", 409)
                            }
                            membership = (await client.query(`INSERT INTO progression_players(session_id,user_id,ticket_hash)
                                VALUES ($1,$2,$3) RETURNING *`, [sessionId, userId, ticketHash])).rows[0]
                        }
                        const user = (await client.query("SELECT username FROM users WHERE id=$1 AND deleted_at IS NULL", [userId])).rows[0]
                        if (!user) reject("user_not_found", 404)
                        await client.query("INSERT INTO game_stats(user_id) VALUES ($1) ON CONFLICT DO NOTHING", [userId])
                        const stats = (await client.query("SELECT * FROM game_stats WHERE user_id=$1", [userId])).rows[0]
                        players.push({ userId, username: user.username, revision: String(membership.revision),
                            ...progressionTotals(stats), cumulative: progressionTotals(membership) })
                    }
                    response = { success: true, sessionId, players }
                } else {
                    const players = body.players.map(player => {
                        if (!player || typeof player !== "object") reject("invalid_player")
                        return { userId: progressionInteger(player.userId, true), revision: progressionInteger(player.revision, true),
                            ...Object.fromEntries(Object.keys(progressionFields).map(key => [key, progressionInteger(player[key])])) }
                    }).sort((a, b) => BigInt(a.userId) < BigInt(b.userId) ? -1 : 1)
                    if (new Set(players.map(player => player.userId)).size !== players.length) reject("duplicate_player")
                    const fingerprint = createHash("sha256").update(JSON.stringify({ players, reason })).digest("hex")
                    const eventId = `progression:${sessionId}:${batchId}`
                    const saved = (await client.query("SELECT result FROM processed_events WHERE event_id=$1", [eventId])).rows[0]
                    if (saved) {
                        if (saved.result.fingerprint !== fingerprint) reject("batch_id_conflict", 409)
                        response = saved.result.response
                    } else {
                        const acknowledgements = []
                        // Numeric user ordering also prevents cross-session totals lock inversion.
                        for (const player of players) {
                            const prior = (await client.query(`SELECT p.* FROM progression_players p
                                JOIN users u ON u.id=p.user_id AND u.deleted_at IS NULL
                                WHERE p.session_id=$1 AND p.user_id=$2`, [sessionId, player.userId])).rows[0]
                            if (!prior) reject("player_not_registered", 403)
                            const newer = BigInt(player.revision) > BigInt(prior.revision)
                            const same = BigInt(player.revision) === BigInt(prior.revision)
                            if (!newer && !same) reject("stale_revision", 409)
                            const deltas = Object.entries(progressionFields).map(([key, column]) => {
                                const delta = BigInt(player[key]) - BigInt(prior[column])
                                if ((newer && delta < 0n) || (same && delta !== 0n)) reject("revision_conflict", 409)
                                return delta.toString()
                            })
                            if (newer) {
                                await client.query(`INSERT INTO game_stats(user_id,${columns.join(",")})
                                    VALUES ($1,${columns.map((_, i) => `$${i + 2}`).join(",")})
                                    ON CONFLICT(user_id) DO UPDATE SET
                                    ${columns.map((column, i) => `${column}=game_stats.${column}+$${i + 2}`).join(",")},updated_at=NOW()`, [player.userId, ...deltas])
                                await client.query(`UPDATE progression_players SET revision=$3,
                                    ${columns.map((column, i) => `${column}=$${i + 4}`).join(",")}
                                    WHERE session_id=$1 AND user_id=$2`, [sessionId, player.userId, player.revision, ...Object.keys(progressionFields).map(key => player[key])])
                            }
                            const stats = (await client.query("SELECT * FROM game_stats WHERE user_id=$1", [player.userId])).rows[0]
                            acknowledgements.push({ userId: player.userId, revision: player.revision, ...progressionTotals(stats) })
                        }
                        response = { success: true, sessionId, batchId, confirmedAt: new Date().toISOString(), players: acknowledgements }
                        await client.query(`INSERT INTO processed_events(event_id,event_type,result)
                            VALUES ($1,'progression_batch',$2)`, [eventId, JSON.stringify({ fingerprint, response })])
                    }
                }
                await client.query("COMMIT")
                // Website process output is captured by the existing mimita-api service log.
                diagnose({ operation, sessionId, batchId, hostId,
                    players: response.players.length, decision: "committed" })
                res.json(response)
            } catch (error) {
                if (client) {
                    try { await client.query("ROLLBACK") } catch { /* Return only the scoped failure below. */ }
                }
                diagnose({ operation, sessionId, batchId, hostId, decision: "rejected", reason: error.progressionCode || error.code || "database_error" })
                if (error.progressionCode) return res.status(error.status).json({ success: false, error: error.progressionCode })
                res.status(500).json({ success: false, error: "progression_save_failed" })
            } finally {
                client?.release()
            }
        })
    }
    return router
}
